/*
 * compat/SDL.h -- a minimal, faithful SDL_Surface stand-in for the Cronopio
 * UQM graphics backend.
 *
 * UQM TFB graphics layer (libs/graphics/) is platform-independent and
 * delegates the actual pixel work to a backend that implements the
 * TFB_DrawCanvas / TFB_Prim contract. The stock backend (libs/graphics/sdl/
 * canvas.c + primitives.c + rotozoom.c) is written against SDL_Surface, but it
 * only uses SDL as a software pixel container plus a handful of blit/fill/
 * format helpers, never the windowing, event, or audio parts of SDL.
 *
 * Rather than rewrite that backend, we vendor this tiny SDL_Surface-compatible
 * layer so the real, battle-tested canvas.c / primitives.c / rotozoom.c
 * compile and run unchanged. The implementation lives in src/sdl_compat.c and
 * operates purely on plain 8bpp-paletted and 32bpp-ARGB buffers; the 32bpp
 * MAIN screen is downsampled to the 8bpp Cronopio framebuffer once per frame
 * by vid_cron.c.
 *
 * This header is selected by the UQM SDL_INCLUDE(SDL.h) macro (which expands
 * to an include of "SDL.h") because compat/ is first on the include path.
 */
#ifndef CRON_COMPAT_SDL_H
#define CRON_COMPAT_SDL_H

#include <stdint.h>
#include <stddef.h>

/* UQM keys off SDL_MAJOR_VERSION; present as SDL2 (per-pixel alpha via
 * blend modes, SDL_SetColorKey(surf, enable, key) signature, etc.). */
#define SDL_MAJOR_VERSION 2
#define SDL_MINOR_VERSION 0
#define SDL_PATCHLEVEL    0

typedef uint8_t  Uint8;
typedef int8_t   Sint8;
typedef uint16_t Uint16;
typedef int16_t  Sint16;
typedef uint32_t Uint32;
typedef int32_t  Sint32;
typedef uint64_t Uint64;
typedef int64_t  Sint64;

#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN  4321
#define SDL_BYTEORDER   SDL_LIL_ENDIAN

#define SDL_ALPHA_OPAQUE      255
#define SDL_ALPHA_TRANSPARENT 0

/* Legacy SDL1 surface-creation flag; the cron shim ignores flags. */
#define SDL_SWSURFACE 0x00000000
#define SDL_SRCALPHA  0x00010000
#define SDL_SRCCOLORKEY 0x00001000

typedef enum { SDL_FALSE = 0, SDL_TRUE = 1 } SDL_bool;

typedef enum {
	SDL_BLENDMODE_NONE  = 0x00000000,
	SDL_BLENDMODE_BLEND = 0x00000001,
	SDL_BLENDMODE_ADD   = 0x00000002,
	SDL_BLENDMODE_MOD   = 0x00000004
} SDL_BlendMode;

typedef struct SDL_Color {
	Uint8 r, g, b, a;
} SDL_Color;

typedef struct SDL_Palette {
	int ncolors;
	SDL_Color *colors;
} SDL_Palette;

typedef struct SDL_PixelFormat {
	Uint8  BitsPerPixel;
	Uint8  BytesPerPixel;
	Uint32 Rmask, Gmask, Bmask, Amask;
	Uint8  Rshift, Gshift, Bshift, Ashift;
	Uint8  Rloss, Gloss, Bloss, Aloss;
	SDL_Palette *palette;
} SDL_PixelFormat;

typedef struct SDL_Rect {
	int x, y;
	int w, h;
} SDL_Rect;

typedef struct SDL_Surface {
	Uint32 flags;
	SDL_PixelFormat *format;
	int w, h;
	int pitch;
	void *pixels;
	SDL_Rect clip_rect;
	int refcount;

	/* --- cron-shim private state (mirrors SDL2 internal surface state) --- */
	int           ckey_enabled;  /* SDL_SetColorKey enable flag */
	Uint32        ckey;          /* colorkey value, in this surface's format */
	SDL_BlendMode blend;         /* SDL_SetSurfaceBlendMode */
	Uint8         alpha_mod;     /* SDL_SetSurfaceAlphaMod (surface alpha) */
	int           owns_pixels;   /* free pixels on SDL_FreeSurface */
} SDL_Surface;

/* ---- surface lifecycle ---- */
SDL_Surface *SDL_CreateRGBSurface (Uint32 flags, int width, int height,
		int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
void SDL_FreeSurface (SDL_Surface *surface);
SDL_Surface *SDL_ConvertSurface (SDL_Surface *src,
		const SDL_PixelFormat *fmt, Uint32 flags);

/* ---- locking (no-op: software surfaces are always accessible) ---- */
int  SDL_LockSurface (SDL_Surface *surface);
void SDL_UnlockSurface (SDL_Surface *surface);

/* ---- pixel format conversion ---- */
Uint32 SDL_MapRGB  (const SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b);
Uint32 SDL_MapRGBA (const SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b,
		Uint8 a);
void SDL_GetRGB  (Uint32 pixel, const SDL_PixelFormat *fmt,
		Uint8 *r, Uint8 *g, Uint8 *b);
void SDL_GetRGBA (Uint32 pixel, const SDL_PixelFormat *fmt,
		Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a);

/* ---- blit / fill ---- */
int SDL_FillRect (SDL_Surface *dst, const SDL_Rect *rect, Uint32 color);
int SDL_BlitSurface (SDL_Surface *src, const SDL_Rect *srcrect,
		SDL_Surface *dst, SDL_Rect *dstrect);
#define SDL_UpperBlit SDL_BlitSurface

/* ---- clip rect ---- */
void     SDL_GetClipRect (SDL_Surface *surface, SDL_Rect *rect);
SDL_bool SDL_SetClipRect (SDL_Surface *surface, const SDL_Rect *rect);

/* ---- colorkey / blend / alpha / palette (SDL2 signatures) ---- */
int SDL_SetColorKey (SDL_Surface *surface, int flag, Uint32 key);
int SDL_GetColorKey (SDL_Surface *surface, Uint32 *key);
int SDL_SetSurfaceRLE (SDL_Surface *surface, int flag);
int SDL_SetSurfaceBlendMode (SDL_Surface *surface, SDL_BlendMode blendMode);
int SDL_GetSurfaceBlendMode (SDL_Surface *surface, SDL_BlendMode *blendMode);
int SDL_SetSurfaceAlphaMod (SDL_Surface *surface, Uint8 alpha);
int SDL_GetSurfaceAlphaMod (SDL_Surface *surface, Uint8 *alpha);
int SDL_SetPaletteColors (SDL_Palette *palette, const SDL_Color *colors,
		int firstcolor, int ncolors);

/* ---- error ---- */
const char *SDL_GetError (void);
void        SDL_SetError (const char *fmt, ...);
void        SDL_ClearError (void);

#endif /* CRON_COMPAT_SDL_H */
