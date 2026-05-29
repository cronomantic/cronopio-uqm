/*
 * img_cron.c — PNG image decode for the Cronopio UQM port.
 *
 * UQM stores its cel/font graphics as PNG files inside the .uqm content packs.
 * The stock backend loads them through SDL_image (png2sdl.c); we don't have
 * SDL_image, so this provides sdluio_loadImage() — the function canvas.c's
 * TFB_DrawCanvas_LoadFromFile() calls — backed by the public-domain stb_image
 * decoder (vendored in Cronopio for tools/2dpak), reading the file bytes
 * through libs/uio (so it transparently inflates entries from the mounted
 * .uqm zip).
 *
 * SLICE 3a: decode to 32bpp ARGB truecolor (stb expands paletted PNGs to RGBA,
 * honouring tRNS / alpha). Fonts (alpha-from-intensity) and truecolor cels work
 * from this. Paletted-cel colormap remapping (ship/planet recolouring, which
 * needs an 8bpp paletted canvas + a per-frame palette) is a later refinement;
 * such cels render with their baked-in colours for now.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "port.h"
#include "libs/uio.h"
#include "libs/log.h"
#include "SDL.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_FAILURE_STRINGS
#define STBI_NO_THREAD_LOCALS   /* CronoVM has no TLS (llvm.threadlocal.address
                                 * is unlowered); use the plain global. */
#define STBI_ASSERT(x) ((void)0)
#include "stb_image.h"

/* ARGB8888 channel layout shared with sdl_compat.c / vid_cron.c */
#define IMG_R_MASK 0x00FF0000u
#define IMG_G_MASK 0x0000FF00u
#define IMG_B_MASK 0x000000FFu
#define IMG_A_MASK 0xFF000000u

SDL_Surface *
sdluio_loadImage (uio_DirHandle *dir, const char *fileName)
{
	uio_Stream *fp;
	long sz;
	unsigned char *raw, *px;
	int w, h, comp, y;
	SDL_Surface *surf;
	size_t rd;

	fp = uio_fopen (dir, fileName, "rb");
	if (!fp)
		return NULL;

	uio_fseek (fp, 0, SEEK_END);
	sz = uio_ftell (fp);
	uio_fseek (fp, 0, SEEK_SET);
	if (sz <= 0) { uio_fclose (fp); return NULL; }

	raw = (unsigned char *)malloc ((size_t)sz);
	if (!raw) { uio_fclose (fp); return NULL; }
	rd = uio_fread (raw, 1, (size_t)sz, fp);
	uio_fclose (fp);
	if (rd != (size_t)sz) { free (raw); return NULL; }

	/* decode to RGBA8888 (4 components) */
	px = stbi_load_from_memory (raw, (int)sz, &w, &h, &comp, 4);
	free (raw);
	if (!px)
		return NULL;

	surf = SDL_CreateRGBSurface (0, w, h, 32,
			IMG_R_MASK, IMG_G_MASK, IMG_B_MASK, IMG_A_MASK);
	if (!surf) { stbi_image_free (px); return NULL; }

	for (y = 0; y < h; ++y)
	{
		uint32_t *drow = (uint32_t *)((uint8_t *)surf->pixels
				+ (size_t)y * surf->pitch);
		const unsigned char *srow = px + (size_t)y * w * 4;
		int x;
		for (x = 0; x < w; ++x)
		{
			unsigned char r = srow[x * 4 + 0];
			unsigned char g = srow[x * 4 + 1];
			unsigned char b = srow[x * 4 + 2];
			unsigned char a = srow[x * 4 + 3];
			drow[x] = ((uint32_t)a << 24) | ((uint32_t)r << 16)
					| ((uint32_t)g << 8) | (uint32_t)b;
		}
	}
	stbi_image_free (px);
	return surf;
}

/* SLICE-3a derisk: load a known PNG straight from the mounted content and log
 * its dimensions, to prove the uio->stb decode path works before wiring the
 * cel/font resource loaders. Removed once the loaders exercise it for real. */
void
cron_img_selftest (uio_DirHandle *dir)
{
	SDL_Surface *s = sdluio_loadImage (dir, "base/lander/lander-002.png");
	if (s)
	{
		log_add (log_User, "IMG: lander-002.png decoded %dx%d", s->w, s->h);
		SDL_FreeSurface (s);
	}
	else
	{
		log_add (log_User, "IMG: PNG decode FAILED");
	}
}
