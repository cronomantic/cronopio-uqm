/*
 * sdl_compat.c — implementation of the minimal SDL_Surface layer (compat/SDL.h)
 * plus the libs/graphics/sdl/sdl_common.c helper surface that UQM's stock
 * canvas.c / primitives.c / rotozoom.c expect.
 *
 * This is the Cronopio UQM graphics backend's foundation: it provides software
 * 8bpp-paletted and 32bpp-ARGB surfaces and the handful of SDL blit/fill/format
 * routines the TFB backend calls, with semantics faithful to SDL2:
 *   - SDL_CreateRGBSurface enables BLEND blending iff the surface has an alpha
 *     mask (matches SDL2, which is why per-pixel-alpha images blend on a plain
 *     SDL_BlitSurface while opaque/colorkeyed surfaces copy).
 *   - SDL_BlitSurface honours colorkey, per-pixel alpha (when blend==BLEND),
 *     and surface alpha mod, converting the source format to the destination.
 *
 * The screen canvases (SDL_Screens[]) are 320x240 32bpp opaque surfaces; the
 * MAIN one is downsampled to the 8bpp Cronopio framebuffer by vid_cron.c.
 */

#include "port.h"
#include "libs/graphics/sdl/sdl_common.h"
#include "libs/log.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* Canonical 32bpp ARGB8888 channel masks (matches vid_cron's downsample). */
#define CRON_R_MASK 0x00FF0000u
#define CRON_G_MASK 0x0000FF00u
#define CRON_B_MASK 0x000000FFu
#define CRON_A_MASK 0xFF000000u

/* ---- backend globals expected by sdl_common.h consumers ---- */
SDL_Surface *SDL_Screen = NULL;
SDL_Surface *TransitionScreen = NULL;
SDL_Surface *SDL_Screens[TFB_GFX_NUMSCREENS] = { NULL, NULL, NULL };
SDL_Surface *format_conv_surf = NULL;
TFB_GRAPHICS_BACKEND *graphics_backend = NULL;

/* ---- error string ---- */
static char s_sdl_error[256];

const char *
SDL_GetError (void)
{
	return s_sdl_error;
}

void
SDL_SetError (const char *fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	vsnprintf (s_sdl_error, sizeof s_sdl_error, fmt, ap);
	va_end (ap);
}

void
SDL_ClearError (void)
{
	s_sdl_error[0] = '\0';
}

/* ---- pixel format helpers ---- */

/* Derive shift (trailing-zero count) and loss (8 - set-bit count) from a
 * channel mask, exactly as SDL does when building an SDL_PixelFormat. */
static void
mask_to_shift_loss (Uint32 mask, Uint8 *shift, Uint8 *loss)
{
	Uint8 s = 0, bits = 0;
	if (mask == 0) { *shift = 0; *loss = 8; return; }
	while (!(mask & 1)) { mask >>= 1; ++s; }
	while (mask & 1)    { mask >>= 1; ++bits; }
	*shift = s;
	*loss = (Uint8)(8 - bits);
}

static SDL_PixelFormat *
make_format (int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
	SDL_PixelFormat *fmt = (SDL_PixelFormat *)calloc (1, sizeof *fmt);
	if (!fmt)
		return NULL;
	fmt->BitsPerPixel = (Uint8)depth;
	fmt->BytesPerPixel = (Uint8)((depth + 7) / 8);
	if (depth == 8)
	{	/* paletted */
		fmt->palette = (SDL_Palette *)calloc (1, sizeof *fmt->palette);
		fmt->palette->ncolors = 256;
		fmt->palette->colors =
				(SDL_Color *)calloc (256, sizeof (SDL_Color));
	}
	else
	{
		fmt->Rmask = Rmask; fmt->Gmask = Gmask;
		fmt->Bmask = Bmask; fmt->Amask = Amask;
		mask_to_shift_loss (Rmask, &fmt->Rshift, &fmt->Rloss);
		mask_to_shift_loss (Gmask, &fmt->Gshift, &fmt->Gloss);
		mask_to_shift_loss (Bmask, &fmt->Bshift, &fmt->Bloss);
		mask_to_shift_loss (Amask, &fmt->Ashift, &fmt->Aloss);
	}
	return fmt;
}

Uint32
SDL_MapRGBA (const SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	if (fmt->palette)
		return 0; /* paletted: not a meaningful direct map */
	return ((Uint32)(r >> fmt->Rloss) << fmt->Rshift)
	     | ((Uint32)(g >> fmt->Gloss) << fmt->Gshift)
	     | ((Uint32)(b >> fmt->Bloss) << fmt->Bshift)
	     | (fmt->Amask ? ((Uint32)(a >> fmt->Aloss) << fmt->Ashift) : 0);
}

Uint32
SDL_MapRGB (const SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b)
{
	return SDL_MapRGBA (fmt, r, g, b, SDL_ALPHA_OPAQUE);
}

/* expand an N-bit channel value to 8 bits, mirroring SDL's reconstruction */
static inline Uint8
expand_channel (Uint32 v, Uint8 loss)
{
	v = v << loss;
	if (loss)
		v |= v >> (8 - loss);
	return (Uint8)v;
}

void
SDL_GetRGBA (Uint32 pixel, const SDL_PixelFormat *fmt,
		Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a)
{
	if (fmt->palette)
	{
		const SDL_Color *c = &fmt->palette->colors[pixel & 0xFF];
		*r = c->r; *g = c->g; *b = c->b; *a = SDL_ALPHA_OPAQUE;
		return;
	}
	*r = expand_channel ((pixel & fmt->Rmask) >> fmt->Rshift, fmt->Rloss);
	*g = expand_channel ((pixel & fmt->Gmask) >> fmt->Gshift, fmt->Gloss);
	*b = expand_channel ((pixel & fmt->Bmask) >> fmt->Bshift, fmt->Bloss);
	*a = fmt->Amask
		? expand_channel ((pixel & fmt->Amask) >> fmt->Ashift, fmt->Aloss)
		: SDL_ALPHA_OPAQUE;
}

void
SDL_GetRGB (Uint32 pixel, const SDL_PixelFormat *fmt,
		Uint8 *r, Uint8 *g, Uint8 *b)
{
	Uint8 a;
	SDL_GetRGBA (pixel, fmt, r, g, b, &a);
}

/* ---- surface lifecycle ---- */

SDL_Surface *
SDL_CreateRGBSurface (Uint32 flags, int width, int height, int depth,
		Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
	SDL_Surface *s;
	(void) flags;

	s = (SDL_Surface *)calloc (1, sizeof *s);
	if (!s)
	{
		SDL_SetError ("out of memory (SDL_Surface, %dx%d@%d)", width, height, depth);
		return NULL;
	}
	s->format = make_format (depth, Rmask, Gmask, Bmask, Amask);
	if (!s->format)
	{
		SDL_SetError ("out of memory (format, %dx%d@%d)", width, height, depth);
		free (s);
		return NULL;
	}
	s->w = width;
	s->h = height;
	s->pitch = ((width * s->format->BytesPerPixel) + 3) & ~3;
	s->pixels = (width && height) ? calloc (1, (size_t)s->pitch * height)
	                              : NULL;
	if (width && height && !s->pixels)
	{
		SDL_SetError ("out of memory (%d bytes for %dx%d@%d pixels)",
				(int)((size_t)s->pitch * height), width, height, depth);
		free (s->format);
		free (s);
		return NULL;
	}
	s->owns_pixels = 1;
	s->clip_rect.x = 0;
	s->clip_rect.y = 0;
	s->clip_rect.w = width;
	s->clip_rect.h = height;
	s->refcount = 1;
	/* SDL2: surfaces with an alpha channel default to BLEND blending. */
	s->blend = Amask ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE;
	s->alpha_mod = SDL_ALPHA_OPAQUE;
	s->ckey_enabled = 0;
	return s;
}

void
SDL_FreeSurface (SDL_Surface *surface)
{
	if (!surface)
		return;
	if (surface->owns_pixels && surface->pixels)
		free (surface->pixels);
	if (surface->format)
	{
		if (surface->format->palette)
		{
			free (surface->format->palette->colors);
			free (surface->format->palette);
		}
		free (surface->format);
	}
	free (surface);
}

int  SDL_LockSurface   (SDL_Surface *surface) { (void) surface; return 0; }
void SDL_UnlockSurface (SDL_Surface *surface) { (void) surface; }

/* ---- clip rect ---- */

void
SDL_GetClipRect (SDL_Surface *surface, SDL_Rect *rect)
{
	if (surface && rect)
		*rect = surface->clip_rect;
}

SDL_bool
SDL_SetClipRect (SDL_Surface *surface, const SDL_Rect *rect)
{
	SDL_Rect full;
	if (!surface)
		return SDL_FALSE;
	full.x = 0; full.y = 0; full.w = surface->w; full.h = surface->h;
	if (!rect)
	{
		surface->clip_rect = full;
		return SDL_TRUE;
	}
	/* intersect requested rect with the surface bounds */
	{
		int x0 = rect->x < 0 ? 0 : rect->x;
		int y0 = rect->y < 0 ? 0 : rect->y;
		int x1 = rect->x + rect->w; if (x1 > surface->w) x1 = surface->w;
		int y1 = rect->y + rect->h; if (y1 > surface->h) y1 = surface->h;
		surface->clip_rect.x = x0;
		surface->clip_rect.y = y0;
		surface->clip_rect.w = (x1 > x0) ? (x1 - x0) : 0;
		surface->clip_rect.h = (y1 > y0) ? (y1 - y0) : 0;
	}
	return (surface->clip_rect.w > 0 && surface->clip_rect.h > 0)
			? SDL_TRUE : SDL_FALSE;
}

/* ---- colorkey / blend / alpha / palette ---- */

int
SDL_SetColorKey (SDL_Surface *surface, int flag, Uint32 key)
{
	if (!surface)
		return -1;
	if (flag)
	{
		surface->ckey_enabled = 1;
		surface->ckey = key;
	}
	else
	{
		surface->ckey_enabled = 0;
	}
	return 0;
}

int
SDL_GetColorKey (SDL_Surface *surface, Uint32 *key)
{
	if (!surface || !surface->ckey_enabled)
		return -1;
	if (key)
		*key = surface->ckey;
	return 0;
}

int SDL_SetSurfaceRLE (SDL_Surface *surface, int flag)
{
	(void) surface; (void) flag; return 0;  /* no RLE acceleration */
}

int
SDL_SetSurfaceBlendMode (SDL_Surface *surface, SDL_BlendMode blendMode)
{
	if (!surface) return -1;
	surface->blend = blendMode;
	return 0;
}

int
SDL_GetSurfaceBlendMode (SDL_Surface *surface, SDL_BlendMode *blendMode)
{
	if (!surface) return -1;
	if (blendMode) *blendMode = surface->blend;
	return 0;
}

int
SDL_SetSurfaceAlphaMod (SDL_Surface *surface, Uint8 alpha)
{
	if (!surface) return -1;
	surface->alpha_mod = alpha;
	return 0;
}

int
SDL_GetSurfaceAlphaMod (SDL_Surface *surface, Uint8 *alpha)
{
	if (!surface) return -1;
	if (alpha) *alpha = surface->alpha_mod;
	return 0;
}

int
SDL_SetPaletteColors (SDL_Palette *palette, const SDL_Color *colors,
		int firstcolor, int ncolors)
{
	int i;
	if (!palette || !colors)
		return -1;
	for (i = 0; i < ncolors && (firstcolor + i) < palette->ncolors; ++i)
		palette->colors[firstcolor + i] = colors[i];
	return 0;
}

/* ---- fill ---- */

int
SDL_FillRect (SDL_Surface *dst, const SDL_Rect *rect, Uint32 color)
{
	SDL_Rect r;
	int y;
	int bpp;

	if (!dst || !dst->pixels)
		return -1;
	bpp = dst->format->BytesPerPixel;

	if (rect)
		r = *rect;
	else
	{
		r.x = 0; r.y = 0; r.w = dst->w; r.h = dst->h;
	}
	/* clip to the surface clip rect */
	{
		const SDL_Rect *c = &dst->clip_rect;
		int x0 = r.x < c->x ? c->x : r.x;
		int y0 = r.y < c->y ? c->y : r.y;
		int x1 = r.x + r.w; if (x1 > c->x + c->w) x1 = c->x + c->w;
		int y1 = r.y + r.h; if (y1 > c->y + c->h) y1 = c->y + c->h;
		r.x = x0; r.y = y0;
		r.w = (x1 > x0) ? (x1 - x0) : 0;
		r.h = (y1 > y0) ? (y1 - y0) : 0;
	}
	if (r.w <= 0 || r.h <= 0)
		return 0;

	for (y = 0; y < r.h; ++y)
	{
		Uint8 *row = (Uint8 *)dst->pixels + (size_t)(r.y + y) * dst->pitch;
		if (bpp == 4)
		{
			Uint32 *p = (Uint32 *)row + r.x;
			int x;
			for (x = 0; x < r.w; ++x)
				p[x] = color;
		}
		else /* bpp == 1 */
		{
			memset (row + r.x, (int)(color & 0xFF), (size_t)r.w);
		}
	}
	return 0;
}

/* ---- blit ---- */

static inline Uint32
read_pixel (const SDL_Surface *s, int x, int y)
{
	const Uint8 *p = (const Uint8 *)s->pixels + (size_t)y * s->pitch
			+ (size_t)x * s->format->BytesPerPixel;
	if (s->format->BytesPerPixel == 4)
		return *(const Uint32 *)p;
	return *p; /* 8bpp */
}

/* SDL_BlitSurface: convert+composite src onto dst. dst is assumed 32bpp
 * (all blit targets in UQM are the 32bpp screens / truecolor images).
 * Honours src colorkey, per-pixel alpha (blend==BLEND), and surface alpha. */
int
SDL_BlitSurface (SDL_Surface *src, const SDL_Rect *srcrect,
		SDL_Surface *dst, SDL_Rect *dstrect)
{
	SDL_Rect sr, dr;
	const SDL_PixelFormat *sf;
	SDL_PixelFormat *df;
	int sx, sy, dx, dy;
	int blend_pp;        /* per-pixel/surface alpha blend active */
	int use_ckey;
	Uint32 ckey;
	int clipx0, clipy0, clipx1, clipy1;

	if (!src || !dst || !src->pixels || !dst->pixels)
		return -1;
	sf = src->format;
	df = dst->format;

	/* source rect (default: whole src) */
	if (srcrect)
		sr = *srcrect;
	else { sr.x = 0; sr.y = 0; sr.w = src->w; sr.h = src->h; }

	/* destination origin (default: 0,0); size comes from src rect */
	if (dstrect) { dr.x = dstrect->x; dr.y = dstrect->y; }
	else         { dr.x = 0; dr.y = 0; }
	dr.w = sr.w; dr.h = sr.h;

	/* clip against dst clip_rect, carrying the offset back to the src rect */
	clipx0 = dst->clip_rect.x;
	clipy0 = dst->clip_rect.y;
	clipx1 = dst->clip_rect.x + dst->clip_rect.w;
	clipy1 = dst->clip_rect.y + dst->clip_rect.h;

	if (dr.x < clipx0) { int d = clipx0 - dr.x; sr.x += d; sr.w -= d; dr.x = clipx0; }
	if (dr.y < clipy0) { int d = clipy0 - dr.y; sr.y += d; sr.h -= d; dr.y = clipy0; }
	if (dr.x + sr.w > clipx1) sr.w = clipx1 - dr.x;
	if (dr.y + sr.h > clipy1) sr.h = clipy1 - dr.y;
	/* also clip against the source bounds */
	if (sr.x < 0) { dr.x -= sr.x; sr.w += sr.x; sr.x = 0; }
	if (sr.y < 0) { dr.y -= sr.y; sr.h += sr.y; sr.y = 0; }
	if (sr.x + sr.w > src->w) sr.w = src->w - sr.x;
	if (sr.y + sr.h > src->h) sr.h = src->h - sr.y;

	if (dstrect) { dstrect->w = sr.w > 0 ? sr.w : 0; dstrect->h = sr.h > 0 ? sr.h : 0; }
	if (sr.w <= 0 || sr.h <= 0)
		return 0;

	use_ckey = src->ckey_enabled;
	ckey = src->ckey;
	blend_pp = (src->blend == SDL_BLENDMODE_BLEND);

	for (sy = 0; sy < sr.h; ++sy)
	{
		Uint32 *drow = (Uint32 *)((Uint8 *)dst->pixels
				+ (size_t)(dr.y + sy) * dst->pitch) + dr.x;
		dy = sy;
		for (sx = 0; sx < sr.w; ++sx)
		{
			Uint32 spix = read_pixel (src, sr.x + sx, sr.y + sy);
			Uint8 r, g, b, a;

			if (use_ckey && spix == ckey)
				continue; /* transparent */

			SDL_GetRGBA (spix, sf, &r, &g, &b, &a);

			if (blend_pp)
			{
				/* effective alpha = per-pixel (if any) modulated by surf alpha,
				 * or surface alpha when the source has no alpha channel */
				int ea = sf->Amask ? a : src->alpha_mod;
				if (sf->Amask && src->alpha_mod != 0xFF)
					ea = (ea * src->alpha_mod) >> 8;
				if (ea <= 0)
					continue;
				if (ea < 255)
				{
					Uint8 dr8, dg8, db8, da8;
					SDL_GetRGBA (drow[sx], df, &dr8, &dg8, &db8, &da8);
					r = (Uint8)((r * ea + dr8 * (255 - ea)) / 255);
					g = (Uint8)((g * ea + dg8 * (255 - ea)) / 255);
					b = (Uint8)((b * ea + db8 * (255 - ea)) / 255);
				}
			}
			(void) dx; (void) dy;
			drow[sx] = SDL_MapRGBA (df, r, g, b, SDL_ALPHA_OPAQUE);
		}
	}
	return 0;
}

/* ---- format conversion ---- */

SDL_Surface *
SDL_ConvertSurface (SDL_Surface *src, const SDL_PixelFormat *fmt, Uint32 flags)
{
	SDL_Surface *dst;
	int x, y;
	(void) flags;

	if (!src || !fmt)
		return NULL;
	dst = SDL_CreateRGBSurface (0, src->w, src->h, fmt->BitsPerPixel,
			fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);
	if (!dst)
		return NULL;

	for (y = 0; y < src->h; ++y)
	{
		for (x = 0; x < src->w; ++x)
		{
			Uint32 sp = read_pixel (src, x, y);
			Uint8 r, g, b, a;
			SDL_GetRGBA (sp, src->format, &r, &g, &b, &a);
			if (dst->format->BytesPerPixel == 4)
			{
				Uint32 *p = (Uint32 *)((Uint8 *)dst->pixels
						+ (size_t)y * dst->pitch) + x;
				*p = SDL_MapRGBA (dst->format, r, g, b, a);
			}
		}
	}

	/* carry over transparency state, converting a colorkey to dst format */
	dst->blend = src->blend;
	dst->alpha_mod = src->alpha_mod;
	if (src->ckey_enabled)
	{
		Uint8 r, g, b, a;
		SDL_GetRGBA (src->ckey, src->format, &r, &g, &b, &a);
		dst->ckey_enabled = 1;
		dst->ckey = SDL_MapRGBA (dst->format, r, g, b, a);
	}
	return dst;
}

/* ---- the sdl_common.c TFB_* surface helper layer (SDL2 semantics) ---- */

int
TFB_HasSurfaceAlphaMod (SDL_Surface *surface)
{
	return surface && surface->blend == SDL_BLENDMODE_BLEND;
}

int
TFB_GetSurfaceAlphaMod (SDL_Surface *surface, Uint8 *alpha)
{
	if (!surface || !alpha)
		return -1;
	*alpha = (surface->blend == SDL_BLENDMODE_BLEND)
			? surface->alpha_mod : 255;
	return 0;
}

int
TFB_SetSurfaceAlphaMod (SDL_Surface *surface, Uint8 alpha)
{
	if (!surface)
		return -1;
	surface->blend = SDL_BLENDMODE_BLEND;
	surface->alpha_mod = alpha;
	return 0;
}

int
TFB_DisableSurfaceAlphaMod (SDL_Surface *surface)
{
	if (!surface)
		return -1;
	surface->alpha_mod = 255;
	surface->blend = SDL_BLENDMODE_NONE;
	return 0;
}

int
TFB_GetColorKey (SDL_Surface *surface, Uint32 *key)
{
	if (!surface || !key)
		return -1;
	return SDL_GetColorKey (surface, key);
}

int
TFB_SetColorKey (SDL_Surface *surface, Uint32 key, int rleaccel)
{
	if (!surface)
		return -1;
	SDL_SetSurfaceRLE (surface, rleaccel);
	return SDL_SetColorKey (surface, SDL_TRUE, key);
}

int
TFB_DisableColorKey (SDL_Surface *surface)
{
	if (!surface)
		return -1;
	return SDL_SetColorKey (surface, SDL_FALSE, 0);
}

int
TFB_HasColorKey (SDL_Surface *surface)
{
	Uint32 key;
	return TFB_GetColorKey (surface, &key) == 0;
}

int
TFB_SetColors (SDL_Surface *surface, SDL_Color *colors, int firstcolor,
		int ncolors)
{
	if (!surface || !colors || !surface->format || !surface->format->palette)
		return 0;
	if (SDL_SetPaletteColors (surface->format->palette, colors, firstcolor,
			ncolors) == 0)
		return 1; /* SDL2 success convention used by callers */
	return 0;
}

SDL_Surface *
TFB_DisplayFormatAlpha (SDL_Surface *surface)
{
	SDL_Surface *newsurf;
	SDL_PixelFormat *dstfmt;
	const SDL_PixelFormat *srcfmt = surface->format;

	if (surface->format->Amask)
		dstfmt = format_conv_surf->format;
	else
		dstfmt = SDL_Screen->format;

	if (srcfmt->BytesPerPixel == dstfmt->BytesPerPixel &&
			srcfmt->Rmask == dstfmt->Rmask &&
			srcfmt->Gmask == dstfmt->Gmask &&
			srcfmt->Bmask == dstfmt->Bmask &&
			srcfmt->Amask == dstfmt->Amask)
		return surface; /* no conversion needed */

	newsurf = SDL_ConvertSurface (surface, dstfmt, surface->flags);
	if (TFB_HasColorKey (surface) && newsurf &&
			TFB_HasColorKey (newsurf) &&
			TFB_HasSurfaceAlphaMod (newsurf))
	{
		TFB_DisableSurfaceAlphaMod (newsurf);
	}
	return newsurf;
}

TFB_Canvas
TFB_GetScreenCanvas (SCREEN screen)
{
	return SDL_Screens[screen];
}

void
UnInit_Screen (SDL_Surface **screen)
{
	if (!screen || !*screen)
		return;
	SDL_FreeSurface (*screen);
	*screen = NULL;
}

/* ---- cron backend bring-up: create the screen + format surfaces ---- */

void
cron_sdl_screens_init (int w, int h)
{
	int i;

	if (format_conv_surf)
		return; /* already initialised */

	format_conv_surf = SDL_CreateRGBSurface (SDL_SWSURFACE, 0, 0, 32,
			CRON_R_MASK, CRON_G_MASK, CRON_B_MASK, CRON_A_MASK);

	for (i = 0; i < TFB_GFX_NUMSCREENS; ++i)
		SDL_Screens[i] = SDL_CreateRGBSurface (SDL_SWSURFACE, w, h, 32,
				CRON_R_MASK, CRON_G_MASK, CRON_B_MASK, 0);

	SDL_Screen = SDL_Screens[TFB_SCREEN_MAIN];
	TransitionScreen = SDL_Screens[TFB_SCREEN_TRANSITION];

	/* build the BYTE*BYTE multiply table used by the bilinear/trilinear
	 * rescalers in canvas.c */
	TFB_DrawCanvas_Initialize ();
}

/* Expose the MAIN screen's 32bpp pixel buffer to vid_cron for downsampling. */
void
cron_sdl_main_buffer (void **pixels, int *w, int *h, int *pitch)
{
	if (SDL_Screen)
	{
		*pixels = SDL_Screen->pixels;
		*w = SDL_Screen->w;
		*h = SDL_Screen->h;
		*pitch = SDL_Screen->pitch;
	}
	else
	{
		*pixels = NULL; *w = *h = *pitch = 0;
	}
}

/* GFX slice-2a self-test: draw a pattern onto the MAIN screen through the
 * REAL backend path (TFB_DrawCanvas_Rect / _Line over canvas.c + primitives.c)
 * to prove the 32bpp canvas rendering reaches the 8bpp framebuffer via
 * vid_cron's downsample. Removed once UQM's own drawing drives the screen. */
void
cron_gfx_selftest (void)
{
	RECT r;
	if (!SDL_Screen)
		return;

	/* dark blue background */
	r.corner.x = 0; r.corner.y = 0;
	r.extent.width = SDL_Screen->w; r.extent.height = SDL_Screen->h;
	TFB_DrawCanvas_Rect (&r, BUILD_COLOR_RGBA (0, 0, 64, 255),
			DRAW_REPLACE_MODE, SDL_Screen);

	/* three filled bars: red / green / white */
	r.corner.x = 20; r.corner.y = 20; r.extent.width = 80; r.extent.height = 40;
	TFB_DrawCanvas_Rect (&r, BUILD_COLOR_RGBA (224, 0, 0, 255),
			DRAW_REPLACE_MODE, SDL_Screen);
	r.corner.x = 120;
	TFB_DrawCanvas_Rect (&r, BUILD_COLOR_RGBA (0, 224, 0, 255),
			DRAW_REPLACE_MODE, SDL_Screen);
	r.corner.x = 220;
	TFB_DrawCanvas_Rect (&r, BUILD_COLOR_RGBA (255, 255, 255, 255),
			DRAW_REPLACE_MODE, SDL_Screen);

	/* a couple of diagonals */
	TFB_DrawCanvas_Line (0, 0, SDL_Screen->w - 1, SDL_Screen->h - 1,
			BUILD_COLOR_RGBA (255, 224, 0, 255), DRAW_REPLACE_MODE, SDL_Screen);
	TFB_DrawCanvas_Line (SDL_Screen->w - 1, 0, 0, SDL_Screen->h - 1,
			BUILD_COLOR_RGBA (0, 224, 255, 255), DRAW_REPLACE_MODE, SDL_Screen);
}

/* ---- rotozoom stubs (sdl/rotozoom.c not compiled; see build_uqm.sh) ----
 * Rotation is only used for melee ship sprites. Until that lands we provide
 * identity stubs so canvas.c's TFB_DrawCanvas_Rotate / _GetRotatedExtent link;
 * a "rotated" image is just a copy of the source at its original size. */
int
rotateSurface (SDL_Surface *src, SDL_Surface *dst, double angle, int smooth)
{
	SDL_Rect r;
	(void) angle; (void) smooth;
	if (!src || !dst)
		return -1;
	r.x = 0; r.y = 0; r.w = src->w; r.h = src->h;
	SDL_BlitSurface (src, &r, dst, &r);
	return 0;
}

void
rotozoomSurfaceSize (int width, int height, double angle, double zoom,
		int *dstwidth, int *dstheight)
{
	(void) angle; (void) zoom;
	if (dstwidth)  *dstwidth = width;
	if (dstheight) *dstheight = height;
}

/* ---- present / mode glue normally provided by the SDL screen driver ---- */

/* TFB_SwapBuffers is the "frame composited" hook the DCQ executor
 * (dcqueue.c TFB_FlushGraphics) calls. The actual present (downsample MAIN ->
 * 8bpp framebuffer) is driven explicitly from the cart's frame() via
 * cron_vid_present(), so this is a no-op. Fade/transition compositing onto the
 * backbuffer (the real driver's job) is deferred to a later slice. */
void
TFB_SwapBuffers (int force_full_redraw)
{
	(void) force_full_redraw;
}

/* We never change video mode at runtime (fixed 320x240 on the Cronopio FB). */
int
TFB_ReInitGraphics (int driver, int flags, int width, int height)
{
	(void) driver; (void) flags; (void) width; (void) height;
	return 0;
}

int
TFB_InitGraphics (int driver, int flags, const char *renderer,
		int width, int height)
{
	(void) driver; (void) flags; (void) renderer;
	cron_sdl_screens_init (width > 0 ? width : 320, height > 0 ? height : 240);
	return 0;
}
