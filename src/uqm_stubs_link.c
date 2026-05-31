/*
 * uqm_stubs_link.c — platform-seam stubs for the FULL-CORE UQM build.
 *
 * Strategy (memory uqm-compile-core-not-stub): the WHOLE UQM game core is
 * compiled (build_uqm.sh's find-based CORE_SRCS — every .c under uqm/ + libs/
 * except the platform backends). The ONLY things stubbed here are the genuine
 * PLATFORM SEAM: the 86 symbols left undefined once the core links, all of
 * which live exclusively in the EXCLUDED backend dirs that Cronopio has no
 * equivalent for —
 *     libs/sound/trackplayer  (speech track + subtitle cursor — 3DO voice pack;
 *                              the music+SFX playback API is REAL now, owned by
 *                              the host-native backend src/snd_cron.c)
 *     libs/video/*            (the legacy FMV player)
 *     libs/input/sdl/*        (the SDL input backend + key config)
 *     libs/graphics/sdl/sdl_common.c (system-rect dirty tracking, gamma,
 *                              transition upload — the cron present is a full
 *                              per-frame downsample, so these are no-ops)
 *     libs/callback/alarm.c   (scheduled alarms — the cart loop doesn't pump them)
 * plus a couple of true platform leaves (SDL_GetTicks, isatty, addon loading).
 *
 * NO game-logic function is stubbed (that was "un error colosal"): the seam set
 * is the exact undefined residual reported by tools/seam_missing.sh, every entry
 * verified to have no owner among the compiled core TUs. If a value-returning
 * game fn ever turns up missing, the fix is to add its owning TU to the build,
 * NEVER to stub it (a garbage return is the ARCTAN landmine).
 *
 * Return contract: in the loosely-typed VM ABI a no-arg `int f(void){return 0;}`
 * is the right no-op for every shape here — a void caller ignores the result, a
 * handle/pointer caller reads 0 == NULL (→ a guarded no-op, e.g. LoadSoundInstance
 * → MenuSounds==NULL → the menu-sound block is skipped), a count/flag caller
 * reads 0 == "nothing playing / not supported / timed out", all correct sentinels.
 */
#include <stdint.h>
#include <stdbool.h>
#include <cronopio.h>           /* cron_time_ms */
#include "port.h"
#include "libs/compiler.h"      /* BOOLEAN */
#include "options.h"            /* INPUT_TEMPLATE, opt* externs + the 3 prototyped
                                 * seam fns (loadAddon/prepareAddons/setGammaCorrection)
                                 * defined with their real signatures below. */

/* ---------------------------------------------------------------------- */
/* Platform DATA globals (normally owned by the excluded backends). Sane,
 * neutral defaults so the reads on the live path are correct, not garbage. */

INPUT_TEMPLATE input_templates[6];   /* key bindings (libs/input/sdl); the cart
                                      * mirrors the pad straight into Immediate
                                      * InputState, so these stay zeroed. */
float   optGamma            = 1.0f;  /* neutral gamma (setupmenu's log() curve) */
int     optSmoothScroll     = 0;     /* PC step-scroll */
BOOLEAN optSubtitles        = TRUE;  /* show comm dialogue text (no voice anyway) */
BOOLEAN optKeepAspectRatio  = TRUE;

/* music/SFX volumes + the whole libs/sound playback API are now owned by the
 * host-native backend src/snd_cron.c (cron_module / cron_ogg / cron_pcm). */

/* ---------------------------------------------------------------------- */
/* True platform leaves with a meaningful value. */

uint32_t SDL_GetTicks (void) { return cron_time_ms (); }   /* host ms counter */
int      isatty       (int fd) { (void)fd; return 0; }      /* never a tty */

/* ---------------------------------------------------------------------- */
/* Seam no-ops (return 0 — see the return contract in the header comment). */
#define SEAM(fn)  int fn (void) { return 0; }

/* ---- libs/sound playback (mixer/music/SFX/channels/volumes) is now REAL:
 *      host-native backend in src/snd_cron.c. No stubs here. ---- */

/* ---- libs/sound/trackplayer: speech track + subtitle cursor ---- */
SEAM (PauseTrack)
SEAM (ResumeTrack)
SEAM (PlayTrack)
SEAM (StopTrack)
SEAM (JumpTrack)
SEAM (PlayingTrack)
SEAM (GetTrackPosition)
SEAM (GetTrackSubtitle)
SEAM (GetTrackSubtitleText)
SEAM (GetFirstTrackSubtitle)
SEAM (GetNextTrackSubtitle)
SEAM (GetLastCharacter)
SEAM (GetNextCharacter)
SEAM (SpliceTrack)
SEAM (SpliceMultiTrack)
SEAM (FastForward_Page)
SEAM (FastForward_Smooth)
SEAM (FastReverse_Page)
SEAM (FastReverse_Smooth)

/* ---- libs/video: the legacy FMV player ---- */
SEAM (InitVideoPlayer)
SEAM (UninitVideoPlayer)
SEAM (InstallVideoResType)
SEAM (LoadLegacyVideoInstance)
SEAM (DestroyLegacyVideo)
SEAM (PlayLegacyVideo)
SEAM (StopLegacyVideo)
SEAM (PlayingLegacyVideo)
SEAM (VidGetPosition)
SEAM (VidProcessFrame)
SEAM (VidSeek)
SEAM (GraphForegroundStream)
SEAM (streamOut)

/* ---- libs/input/sdl: the SDL input backend + key config ---- */
SEAM (BeginInputFrame)
SEAM (InterrogateInputState)
SEAM (RebindInputState)
SEAM (RemoveInputState)
SEAM (EnterCharacterMode)
SEAM (ExitCharacterMode)
SEAM (SaveKeyConfiguration)
SEAM (TFB_ResetControls)

/* ---- libs/graphics/sdl/sdl_common.c: dirty-rect / gamma / transition ---- */
SEAM (SetSystemRect)
SEAM (ClearSystemRect)
SEAM (TFB_UploadTransitionScreen)
SEAM (TFB_SetGamma)
SEAM (TFB_SupportsHardwareScaling)

/* ---- libs/callback/alarm.c: scheduled alarms (loop doesn't pump them) ---- */
SEAM (Alarm_addAbsoluteMs)
SEAM (Alarm_remove)

/* ---- the 3 seam fns with real prototypes in options.h (signatures must
 *      match, so they can't use the no-arg SEAM macro). ---- */
BOOLEAN loadAddon        (const char *addon) { (void)addon; return 0; }   /* no addons */
void    prepareAddons    (const char **addons) { (void)addons; }
bool    setGammaCorrection (float gamma) { (void)gamma; return 0; }       /* no gamma backend */
