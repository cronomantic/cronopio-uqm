/*
 * snd_cron.c — host-native audio backend for UQM on Cronopio.
 *
 * Replaces the ENTIRE libs/sound playback engine (the MixSDL software mixer +
 * the ogg/mod/wav decoders + the SDL/OpenAL device driver) with the Cronopio
 * host audio. Synthesis/decoding lives in the HOST — the interpreted VM is too
 * slow for per-sample DSP (this is why cron_module/libxmp + cron_ogg/stb_vorbis
 * were added host-side; see memory audio-music-architecture). Routing:
 *
 *   music .mod/.s3m/.xm/.it  ->  cron_module_play  (libxmp, host)
 *   music .ogg (3DO remix)   ->  cron_ogg_play     (stb_vorbis, host)
 *   SFX banks (.wav lists)   ->  cron_pcm          (8-bit PCM voices, host)
 *
 * libs/sound/* is NOT compiled. The resource + string-table glue (libs/resource,
 * libs/strings) IS compiled: we register MUSICRES/SNDRES handlers that slurp the
 * raw file straight out of the mounted .uqm via uio and hand the bytes to the
 * host players (music) or pre-decode the wav to 8-bit PCM (SFX). This file owns
 * UQM's game-facing sndlib.h API.
 *
 * DEFERRED (kept no-op in uqm_stubs_link.c): the speech/subtitle trackplayer
 * (3DO voice pack — base content has no speech) and the FMV video player.
 *
 * Known v1 simplifications (faithful enough; revisit if a path needs it):
 *   - FadeMusic / PLRPause set the volume immediately (no smooth ramp/seek).
 *   - PLRPlaying / ChannelPlaying are tracked cart-side (the host exposes no
 *     per-stream "is playing" query): music = "playing until stopped", SFX = a
 *     duration timer. Fine for continuous themes + one-shot stings.
 *   - WaitForSoundEnd returns immediately (the cart must not busy-block).
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <cronopio.h>               /* cron_module_*, cron_ogg_*, cron_pcm, cron_time_ms */

#include "port.h"
#include "libs/compiler.h"          /* BOOLEAN, TRUE/FALSE */
#include "libs/sndlib.h"
#include "libs/strlib.h"
#include "strings/strintrn.h"       /* AllocStringTable, struct string_table layout */
#include "libs/reslib.h"
#include "libs/memlib.h"            /* HMalloc/HFree */
#include "libs/uio.h"
#include "libs/log.h"
#include "libs/timelib.h"           /* GetTimeCounter (seam -> cron_time_ms) */
#include "options.h"                /* contentDir, optStereoSFX */

/* sndlib.h forward-declares `struct tfb_soundsample`; we own its definition. */
enum { SND_KIND_MODULE = 0, SND_KIND_OGG, SND_KIND_PCM };
struct tfb_soundsample
{
	int      kind;
	void    *data;     /* HMalloc'd: raw module/ogg bytes, or decoded 8-bit PCM */
	uint32_t len;      /* bytes in `data` */
	uint32_t rate;     /* PCM only: native sample rate (Hz) */
};

/* ---- volume globals (normally libs/sound/sound.c; declared in sound.h) ---- */
int   musicVolume       = NORMAL_VOLUME; /* 0..MAX_VOLUME; stock sound.c default
                                          * (options.c, which would load it from
                                          * config, isn't compiled — so 0 here = a
                                          * silent mix; NORMAL_VOLUME = audible). */
float musicVolumeScale  = 1.0f;
float sfxVolumeScale    = 1.0f;
float speechVolumeScale = 1.0f;

/* ---- music state (one stream at a time, mirroring the host) ---- */
static MUSIC_REF g_cur_music   = NULL;
static int       g_music_kind  = -1;   /* SND_KIND_* currently on the host, or -1 */
static int       g_music_on    = 0;    /* 0/1: playing (not stopped) */

/* ---- SFX channels -> cron voices (music uses module/ogg engines, not voices,
 *      so all voices are free for SFX). One reused scratch PCM slot: the host
 *      snapshots the sample descriptor into the voice at trigger (apu.c), so a
 *      single slot serves concurrent one-shots. ---- */
#define SND_SCRATCH_SLOT  0
static void     *g_chan_posobj[MAX_CHANNELS];
static int       g_chan_vol[MAX_CHANNELS];     /* 0..255 host gain */
static int       g_chan_pan[MAX_CHANNELS];     /* -128..127 */
static uint32_t  g_chan_end_ms[MAX_CHANNELS];  /* host ms when the one-shot ends */

/* ====================================================================== */
/* helpers                                                                */

static int
music_q8 (void)
{	/* UQM 0..255 * scale -> host Q8 (0..256) */
	float v = (musicVolume / (float) MAX_VOLUME) * musicVolumeScale;
	int q = (int) (v * 256.0f + 0.5f);
	return q < 0 ? 0 : (q > 256 ? 256 : q);
}

static void
apply_music_volume (void)
{
	int q = music_q8 ();
	cron_module_volume (q);
	cron_ogg_volume (q);
}

static void
stop_music (void)
{
	if (g_music_kind == SND_KIND_OGG)
		cron_ogg_stop ();
	else if (g_music_kind == SND_KIND_MODULE)
		cron_module_stop ();
	g_music_kind = -1;
	g_music_on = 0;
}

static int
ext_is_ogg (const char *path)
{
	size_t n = strlen (path);
	return n >= 4 && (path[n-3] == 'o' || path[n-3] == 'O')
	             &&  (path[n-2] == 'g' || path[n-2] == 'G')
	             &&  (path[n-1] == 'g' || path[n-1] == 'G');
}

static int
pan_from_pos (SoundPosition pos)
{
	int pan;
	if (!optStereoSFX || !pos.positional)
		return 0;
	/* world x (~ +/-160 across the screen) -> stereo pan */
	pan = pos.x * 127 / 160;
	return pan < -128 ? -128 : (pan > 127 ? 127 : pan);
}

/* Slurp a whole content file (out of the mounted .uqm) into a fresh HMalloc'd
 * buffer. Caller owns it. Returns NULL (len 0) on any failure.
 *
 * Reads INCREMENTALLY (grow-on-demand), never SEEK_END/ftell: most .uqm entries
 * are DEFLATED, and a deflated uio stream can't report its uncompressed size by
 * seeking to the end (you'd have to inflate it first) — SEEK_END returns 0 and
 * the file looks empty. (Stored entries like the small PNGs happen to seek fine,
 * which is why img_cron's seek-based reader works for those but not for music.) */
static void *
slurp_content (const char *path, uint32_t *out_len)
{
	uio_Stream *fp;
	size_t cap = 65536, len = 0;
	uint8_t *buf;

	*out_len = 0;
	fp = uio_fopen (contentDir, path, "rb");
	if (!fp)
		return NULL;
	buf = HMalloc (cap);
	if (!buf)
	{
		uio_fclose (fp);
		return NULL;
	}
	for (;;)
	{
		size_t got;
		if (len == cap)
		{
			uint8_t *nb = HRealloc (buf, cap * 2);
			if (!nb) { HFree (buf); uio_fclose (fp); return NULL; }
			buf = nb;
			cap *= 2;
		}
		got = uio_fread (buf + len, 1, cap - len, fp);
		len += got;
		if (got == 0)
			break;                         /* EOF (or error) — done */
	}
	uio_fclose (fp);
	if (len == 0)
	{
		HFree (buf);
		return NULL;
	}
	*out_len = (uint32_t) len;
	return buf;
}

/* ====================================================================== */
/* WAV -> 8-bit signed mono PCM (SFX path)                                */
/* UQM sound effects are small PCM .wav files. cron_sample takes 8-bit    */
/* signed mono; cron_pcm resamples the native rate to the device rate.    */

static uint32_t
rd_u32le (const uint8_t *p) { return p[0] | (p[1]<<8) | (p[2]<<16) | ((uint32_t)p[3]<<24); }
static uint16_t
rd_u16le (const uint8_t *p) { return (uint16_t) (p[0] | (p[1]<<8)); }

/* Returns HMalloc'd int8 PCM (mono), *out_len samples, *out_rate Hz; NULL fail. */
static void *
wav_to_pcm8 (const uint8_t *w, uint32_t wlen, uint32_t *out_len, uint32_t *out_rate)
{
	uint32_t pos;
	uint16_t fmt = 0, chans = 1, bits = 8;
	uint32_t rate = 22050;
	const uint8_t *data = NULL;
	uint32_t data_sz = 0;

	*out_len = 0;
	*out_rate = 0;
	if (wlen < 12 || memcmp (w, "RIFF", 4) != 0 || memcmp (w + 8, "WAVE", 4) != 0)
		return NULL;

	pos = 12;
	while (pos + 8 <= wlen)
	{
		uint32_t csz = rd_u32le (w + pos + 4);
		const uint8_t *body = w + pos + 8;
		if (pos + 8 + csz > wlen)
			csz = wlen - (pos + 8);          /* clamp a truncated final chunk */
		if (memcmp (w + pos, "fmt ", 4) == 0 && csz >= 16)
		{
			fmt   = rd_u16le (body + 0);
			chans = rd_u16le (body + 2);
			rate  = rd_u32le (body + 4);
			bits  = rd_u16le (body + 14);
		}
		else if (memcmp (w + pos, "data", 4) == 0)
		{
			data = body;
			data_sz = csz;
		}
		pos += 8 + csz + (csz & 1);              /* chunks are word-aligned */
	}

	if (!data || !data_sz || fmt != 1 /* PCM */ || chans < 1
			|| (bits != 8 && bits != 16) || rate == 0)
		return NULL;

	{
		uint32_t frame_bytes = (uint32_t) chans * (bits / 8);
		uint32_t frames = data_sz / frame_bytes;
		int8_t *out;
		uint32_t i, c;

		if (!frames)
			return NULL;
		out = HMalloc (frames);
		if (!out)
			return NULL;

		for (i = 0; i < frames; ++i)
		{
			const uint8_t *fr = data + (size_t) i * frame_bytes;
			int acc = 0;
			for (c = 0; c < chans; ++c)
			{
				if (bits == 16)
					acc += (int16_t) rd_u16le (fr + c * 2) >> 8;  /* -> s8 */
				else
					acc += (int) fr[c] - 128;                     /* u8 -> s8 */
			}
			acc /= (int) chans;                                   /* downmix */
			out[i] = (int8_t) (acc < -128 ? -128 : (acc > 127 ? 127 : acc));
		}
		*out_len = frames;
		*out_rate = rate;
		return out;
	}
}

/* ====================================================================== */
/* loaders                                                                */

static MUSIC_REF
load_music_file (const char *path)
{
	uint32_t len = 0;
	void *bytes;
	TFB_SoundSample *s;
	MUSIC_REF h;

	bytes = slurp_content (path, &len);
	if (!bytes)
	{
		log_add (log_Warning, "snd_cron: music '%s' not found", path);
		return NULL;
	}
	s = HMalloc (sizeof (*s));
	if (!s) { HFree (bytes); return NULL; }
	s->kind = ext_is_ogg (path) ? SND_KIND_OGG : SND_KIND_MODULE;
	s->data = bytes;
	s->len  = len;
	s->rate = 0;

	h = HMalloc (sizeof (TFB_SoundSample *));   /* MUSIC_REF = TFB_SoundSample** */
	if (!h) { HFree (bytes); HFree (s); return NULL; }
	*h = s;
	return h;
}

static BOOLEAN
free_music_ref (MUSIC_REF h)
{
	TFB_SoundSample *s;

	if (!h)
		return FALSE;
	if (h == g_cur_music)
	{
		stop_music ();
		g_cur_music = NULL;
	}
	s = *h;
	if (s)
	{
		if (s->data)
			HFree (s->data);
		HFree (s);
	}
	HFree (h);
	return TRUE;
}

/* Build a SFX sound bank (SOUND_REF = STRING_TABLE; each entry's data is a
 * TFB_SoundSample**), mirroring _GetSoundBankData: the .snd is a text list of
 * wav filenames relative to the .snd's own directory. */
static SOUND_REF
load_sound_bank (const char *path)
{
#define SND_MAX_FX 256
	uint32_t snd_len = 0;
	char *snd = slurp_content (path, &snd_len);
	TFB_SoundSample *fx[SND_MAX_FX];
	int n_fx = 0;
	int dir_len;
	const char *slash;
	uint32_t off;
	STRING_TABLE tab;
	int i;

	if (!snd)
		return NULL;

	/* directory prefix = everything up to and including the last '/' */
	slash = strrchr (path, '/');
	dir_len = slash ? (int) (slash - path) + 1 : 0;

	/* Walk the text buffer line by line (one wav filename per line). No
	 * strtok_r in picolibc, and a manual splitter keeps zero static state. */
	for (off = 0; off < snd_len && n_fx < SND_MAX_FX; )
	{
		char wavname[256];
		char full[512];
		uint32_t wlen = 0, plen = 0, prate = 0;
		void *wav, *pcm;
		TFB_SoundSample *s;
		uint32_t start = off, llen;

		while (off < snd_len && snd[off] != '\n' && snd[off] != '\r')
			++off;
		llen = off - start;
		while (off < snd_len && (snd[off] == '\n' || snd[off] == '\r'))
			++off;                                  /* consume the EOL run */
		if (llen == 0)
			continue;
		if (llen > sizeof (wavname) - 1)
			llen = sizeof (wavname) - 1;
		memcpy (wavname, snd + start, llen);
		wavname[llen] = '\0';
		{	/* trim to the first whitespace-delimited token */
			char tok[256];
			if (sscanf (wavname, "%255s", tok) != 1 || tok[0] == '\0')
				continue;
			memcpy (wavname, tok, sizeof (tok));
		}

		if (dir_len > 0 && dir_len < (int) sizeof (full))
			memcpy (full, path, dir_len);
		else
			dir_len = 0;
		snprintf (full + dir_len, sizeof (full) - dir_len, "%s", wavname);

		wav = slurp_content (full, &wlen);
		if (!wav)
		{
			log_add (log_Warning, "snd_cron: sfx '%s' not found", full);
			continue;
		}
		pcm = wav_to_pcm8 (wav, wlen, &plen, &prate);
		HFree (wav);
		if (!pcm)
		{
			log_add (log_Warning, "snd_cron: sfx '%s' decode failed", full);
			continue;
		}
		s = HMalloc (sizeof (*s));
		if (!s) { HFree (pcm); continue; }
		s->kind = SND_KIND_PCM;
		s->data = pcm;
		s->len  = plen;
		s->rate = prate;
		fx[n_fx++] = s;
	}
	HFree (snd);

	if (!n_fx)
		return NULL;

	tab = AllocStringTable (n_fx, 0);
	if (!tab)
	{
		while (n_fx--)
		{
			HFree (fx[n_fx]->data);
			HFree (fx[n_fx]);
		}
		return NULL;
	}
	for (i = 0; i < n_fx; ++i)
	{
		TFB_SoundSample **slot = HMalloc (sizeof (TFB_SoundSample *));
		*slot = fx[i];
		tab->strings[i].data = (STRINGPTR) slot;
		tab->strings[i].length = sizeof (TFB_SoundSample *);
	}
	return (SOUND_REF) tab;
}

static BOOLEAN
free_sound_bank (SOUND_REF ref)
{
	STRING_TABLE tab = (STRING_TABLE) ref;
	int i;

	if (!tab)
		return FALSE;
	for (i = 0; i < tab->size; ++i)
	{
		TFB_SoundSample **slot = (TFB_SoundSample **) tab->strings[i].data;
		if (slot && *slot)
		{
			if ((*slot)->data)
				HFree ((*slot)->data);
			HFree (*slot);
		}
		/* `slot` itself is freed by FreeStringTable (it owns strings[].data) */
	}
	FreeStringTable (tab);
	return TRUE;
}

/* ---- resource type vectors ---- */
static void
musicres_load (const char *pathname, RESOURCE_DATA *resdata)
{
	resdata->ptr = load_music_file (pathname);
}
static void
sndres_load (const char *pathname, RESOURCE_DATA *resdata)
{
	resdata->ptr = load_sound_bank (pathname);
}
static BOOLEAN
musicres_free (void *data) { return free_music_ref ((MUSIC_REF) data); }
static BOOLEAN
sndres_free  (void *data) { return free_sound_bank ((SOUND_REF) data); }

/* ====================================================================== */
/* sndlib.h — game-facing API                                             */

int  initAudio (int driver, int flags) { (void) driver; (void) flags; return 0; }

BOOLEAN
InitSound (int argc, char *argv[])
{
	int i;
	(void) argc; (void) argv;
	for (i = 0; i < MAX_CHANNELS; ++i)
	{
		g_chan_posobj[i] = NULL;
		g_chan_vol[i] = MAX_VOLUME;
		g_chan_pan[i] = 0;
		g_chan_end_ms[i] = 0;
	}
	return TRUE;
}

void UninitSound (void) { stop_music (); }

BOOLEAN
InstallAudioResTypes (void)
{
	InstallResTypeVectors ("SNDRES",   sndres_load,   sndres_free,   NULL);
	InstallResTypeVectors ("MUSICRES", musicres_load, musicres_free, NULL);
	return TRUE;
}

SOUND_REF LoadSoundFile (const char *pStr) { return load_sound_bank (pStr); }
MUSIC_REF LoadMusicFile (const char *pStr) { return load_music_file (pStr); }

SOUND_REF
LoadSoundInstance (RESOURCE res)
{
	void *h = res_GetResource (res);
	if (h)
		res_DetachResource (res);
	return (SOUND_REF) h;
}

MUSIC_REF
LoadMusicInstance (RESOURCE res)
{
	void *h = res_GetResource (res);
	if (h)
		res_DetachResource (res);
	return (MUSIC_REF) h;
}

BOOLEAN DestroySound (SOUND_REF ref) { return free_sound_bank (ref); }
BOOLEAN DestroyMusic (MUSIC_REF ref) { return free_music_ref (ref); }

SOUNDPTR GetSoundAddress (SOUND sound) { return GetStringAddress (sound); }

/* ---- music transport ---- */
void
PLRPlaySong (MUSIC_REF MusicRef, BOOLEAN Continuous, BYTE Priority)
{
	TFB_SoundSample *s;
	(void) Priority;
	if (!MusicRef || !*MusicRef)
		return;
	s = *MusicRef;
	/* Switch streams by replacing in-place on the SAME host engine — do NOT
	 * cron_*_stop() it first: stop + play in the same audio block race (the host
	 * adopts the new track then a stale stop_req frees it), which silently kills
	 * the new song. cron_module_play / cron_ogg_play already replace whatever is
	 * playing on their engine. Only stop the OTHER engine when crossing kinds. */
	if (s->kind == SND_KIND_OGG)
	{
		if (g_music_kind == SND_KIND_MODULE)
			cron_module_stop ();
		cron_ogg_play (s->data, (int) s->len, Continuous ? 1 : 0);
	}
	else
	{
		if (g_music_kind == SND_KIND_OGG)
			cron_ogg_stop ();
		cron_module_play (s->data, (int) s->len, Continuous ? 1 : 0);
	}
	g_music_kind = s->kind;
	g_cur_music = MusicRef;
	g_music_on = 1;
	apply_music_volume ();
}

void
PLRStop (MUSIC_REF MusicRef)
{
	if (MusicRef && MusicRef == g_cur_music)
	{
		stop_music ();
		g_cur_music = NULL;
	}
}

BOOLEAN
PLRPlaying (MUSIC_REF MusicRef)
{
	if (MusicRef)
		return (MusicRef == g_cur_music && g_music_on) ? TRUE : FALSE;
	return g_music_on ? TRUE : FALSE;
}

void PLRSeek (MUSIC_REF MusicRef, DWORD pos) { (void) MusicRef; (void) pos; }

void
PLRPause (MUSIC_REF MusicRef)
{	/* no host pause; mute (volume 0) approximates it without losing the stream */
	(void) MusicRef;
	cron_module_volume (0);
	cron_ogg_volume (0);
}

void PLRResume (MUSIC_REF MusicRef) { (void) MusicRef; apply_music_volume (); }

void
SetMusicVolume (COUNT Volume)
{
	musicVolume = Volume;
	apply_music_volume ();
}

DWORD
FadeMusic (BYTE end_vol, SIZE TimeInterval)
{	/* v1: set the target immediately; report the nominal fade-end time so
	 * callers that SleepThreadUntil(FadeMusic(...)) still pace correctly. */
	musicVolume = end_vol;
	apply_music_volume ();
	return GetTimeCounter () + (TimeInterval > 0 ? (DWORD) TimeInterval : 0);
}

/* ---- speech (3DO voice pack) — deferred ---- */
void SetSpeechVolume (float volume) { speechVolumeScale = volume; }
void snd_PlaySpeech (MUSIC_REF SpeechRef) { (void) SpeechRef; }
void snd_StopSpeech (void) { }

/* ---- SFX channels ---- */
void
PlayChannel (COUNT channel, SOUND snd, SoundPosition pos,
		void *positional_object, unsigned char priority)
{
	SOUNDPTR sp;
	TFB_SoundSample *s;
	int vol;
	(void) priority;

	if (channel >= MAX_CHANNELS)
		return;
	sp = GetSoundAddress (snd);
	if (!sp)
		return;
	s = *(TFB_SoundSample **) sp;
	if (!s || s->kind != SND_KIND_PCM || !s->data || !s->len || !s->rate)
		return;

	g_chan_posobj[channel] = positional_object;
	g_chan_pan[channel] = pan_from_pos (optStereoSFX ? pos
			: (SoundPosition) { FALSE, 0, 0 });
	vol = (int) (MAX_VOLUME * sfxVolumeScale);
	if (vol > 255) vol = 255;
	g_chan_vol[channel] = vol;

	/* host snapshots the descriptor into the voice at trigger, so the single
	 * scratch slot is safe to reuse across concurrent channels (apu.c). */
	cron_sample (SND_SCRATCH_SLOT, (const int8_t *) s->data, (int) s->len,
			(int) s->rate);
	cron_pcm (channel, SND_SCRATCH_SLOT, CRON_PITCH_1X, vol,
			g_chan_pan[channel], 0);
	g_chan_end_ms[channel] =
			cron_time_ms () + (uint32_t) ((uint64_t) s->len * 1000 / s->rate);
}

void
StopChannel (COUNT channel, BYTE Priority)
{
	(void) Priority;
	if (channel >= MAX_CHANNELS)
		return;
	cron_note_off (channel);
	g_chan_end_ms[channel] = 0;
}

BOOLEAN
ChannelPlaying (COUNT WhichChannel)
{
	if (WhichChannel >= MAX_CHANNELS)
		return FALSE;
	return cron_time_ms () < g_chan_end_ms[WhichChannel] ? TRUE : FALSE;
}

void *
GetPositionalObject (COUNT channel)
{
	return channel < MAX_CHANNELS ? g_chan_posobj[channel] : NULL;
}

void
SetPositionalObject (COUNT channel, void *positional_object)
{
	if (channel < MAX_CHANNELS)
		g_chan_posobj[channel] = positional_object;
}

void
UpdateSoundPosition (COUNT channel, SoundPosition pos)
{
	if (channel >= MAX_CHANNELS)
		return;
	g_chan_pan[channel] = pan_from_pos (pos);
	cron_pcm_params (channel, g_chan_vol[channel], g_chan_pan[channel]);
}

void
SetChannelVolume (COUNT channel, COUNT volume, BYTE priority)
{
	int vol;
	(void) priority;
	if (channel >= MAX_CHANNELS)
		return;
	vol = (int) ((volume / (float) MAX_VOLUME) * sfxVolumeScale * 255.0f);
	if (vol < 0) vol = 0; if (vol > 255) vol = 255;
	g_chan_vol[channel] = vol;
	cron_pcm_params (channel, vol, g_chan_pan[channel]);
}

void
StopSound (void)
{	/* stop all SFX channels (music is a separate host stream) */
	int i;
	for (i = 0; i < MAX_CHANNELS; ++i)
	{
		cron_note_off (i);
		g_chan_end_ms[i] = 0;
	}
}

BOOLEAN
SoundPlaying (void)
{
	int i;
	uint32_t now = cron_time_ms ();
	for (i = 0; i < MAX_CHANNELS; ++i)
		if (now < g_chan_end_ms[i])
			return TRUE;
	return FALSE;
}

void WaitForSoundEnd (COUNT Channel) { (void) Channel; }
