/*
 * vid_cron.c — Cronopio graphics backend present stage for UQM.
 *
 * UQM's TFB graphics layer composes in 32bpp truecolor onto the screen
 * canvases (SDL_Screens[], created by the SDL_Surface shim — see sdl_compat.c).
 * Cronopio's framebuffer is 320x240 8bpp indexed. Once per host frame, this
 * downsamples the MAIN screen to 8bpp via a fixed RGB332 palette (a pure
 * bit-extraction, no per-pixel nearest-colour search) and cron_blit()s it.
 *
 * UQM is 320x240 native = the FB size, so no scaling is needed. The 32bpp
 * screen buffer is owned by the shim; we just read it here.
 */

#include <stdint.h>
#include <stddef.h>
#include <cronopio.h>

#include "vid_cron.h"

#define VID_W 320
#define VID_H 240

/* shim hooks (src/sdl_compat.c) */
extern void cron_sdl_screens_init (int w, int h);
extern void cron_sdl_main_buffer (void **pixels, int *w, int *h, int *pitch);

static uint8_t s_fb8[VID_W * VID_H];   /* downsample target */
static int     s_inited;

int cron_vid_width  (void) { return VID_W; }
int cron_vid_height (void) { return VID_H; }

/* 32bpp ARGB -> RGB332 palette index (pure bit extraction). */
static inline uint8_t rgb332 (uint32_t argb) {
    uint32_t r = (argb >> 16) & 0xFF;
    uint32_t g = (argb >> 8)  & 0xFF;
    uint32_t b =  argb        & 0xFF;
    return (uint8_t)(((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6));
}

void cron_vid_init (void) {
    if (s_inited) return;

    /* Program the framebuffer palette as the RGB332 ramp, so the downsampled
     * index i decodes back to the colour rgb332() quantised it from. */
    for (int i = 0; i < 256; ++i) {
        uint32_t r3 = (i >> 5) & 7, g3 = (i >> 2) & 7, b2 = i & 3;
        uint32_t R = r3 * 255u / 7u, G = g3 * 255u / 7u, B = b2 * 255u / 3u;
        cron_palette_set (i, (R << 16) | (G << 8) | B);
    }

    /* Create the 32bpp TFB screen canvases (MAIN/EXTRA/TRANSITION). */
    cron_sdl_screens_init (VID_W, VID_H);

    s_inited = 1;
}

/* Downsample the MAIN screen 32bpp -> 8bpp RGB332 and blit to the FB. Called
 * once per host frame from the cart's frame() after the scheduler runs and the
 * draw-command queue has been flushed onto the screen canvas. */
void cron_vid_present (void) {
    void *pixels; int w, h, pitch;
    if (!s_inited)
        return;
    cron_sdl_main_buffer (&pixels, &w, &h, &pitch);
    if (!pixels)
        return;
    for (int y = 0; y < h && y < VID_H; ++y) {
        const uint32_t *src = (const uint32_t *)((const uint8_t *)pixels
                + (size_t)y * pitch);
        uint8_t *dst = s_fb8 + (size_t)y * VID_W;
        for (int x = 0; x < w && x < VID_W; ++x)
            dst[x] = rgb332 (src[x]);
    }
    cron_blit (s_fb8, VID_W, VID_H, 0, 0);
    cron_present ();
}
