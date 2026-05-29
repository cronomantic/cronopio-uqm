/*
 * vid_cron.c — Cronopio graphics backend for UQM (slice 1: present pipeline).
 *
 * UQM's TFB graphics layer composes in 32bpp truecolor. Cronopio's framebuffer
 * is 320x240 8bpp indexed. This backend keeps UQM's screens as 32bpp canvases
 * and, once per host frame, downsamples the MAIN screen to 8bpp via a fixed
 * RGB332 palette (a pure bit-extraction, no per-pixel nearest-colour search)
 * and cron_blit()s it to the framebuffer.
 *
 * SLICE 1 (this file, for now): the screen canvases + the present/downsample
 * pipeline + the RGB332 palette, proven end-to-end with a test pattern. The
 * UQM TFB draw entry points (the TFB_DrawScreen, TFB_DrawImage, TFB_Prim and
 * canvas-allocator functions) are still the no-op link stubs; wiring those to
 * draw onto these screen canvases is the next slice. UQM is 320x240 native,
 * exactly the FB size, so no scaling is needed.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <cronopio.h>

#include "vid_cron.h"

#define VID_W 320
#define VID_H 240

/* The three TFB screens (MAIN / EXTRA / TRANSITION), 32bpp ARGB8888. */
static uint32_t *s_screen[3];
static uint8_t   s_fb8[VID_W * VID_H];   /* downsample target */
static int       s_inited;

uint32_t *cron_vid_screen (int which) {
    return (which >= 0 && which < 3) ? s_screen[which] : (uint32_t *)0;
}
int cron_vid_width (void)  { return VID_W; }
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
    for (int i = 0; i < 3; ++i) {
        s_screen[i] = (uint32_t *)malloc ((size_t)VID_W * VID_H * 4);
        if (s_screen[i]) memset (s_screen[i], 0, (size_t)VID_W * VID_H * 4);
    }
    /* Program the framebuffer palette as the RGB332 ramp, so the downsampled
     * index i decodes back to the colour rgb332() quantised it from. */
    for (int i = 0; i < 256; ++i) {
        uint32_t r3 = (i >> 5) & 7, g3 = (i >> 2) & 7, b2 = i & 3;
        uint32_t R = r3 * 255u / 7u, G = g3 * 255u / 7u, B = b2 * 255u / 3u;
        cron_palette_set (i, (R << 16) | (G << 8) | B);
    }
    s_inited = 1;

    /* SLICE-1 TEST: a smooth gradient on MAIN proves the 32bpp->8bpp->FB
     * pipeline reaches the framebuffer (removed once real draws land). */
    if (s_screen[0]) {
        for (int y = 0; y < VID_H; ++y)
            for (int x = 0; x < VID_W; ++x)
                s_screen[0][y * VID_W + x] =
                        ((uint32_t)(x * 255 / VID_W) << 16) |
                        ((uint32_t)(y * 255 / VID_H) << 8)  | 0x80u;
    }
}

/* Downsample the MAIN screen 32bpp -> 8bpp RGB332 and blit to the FB. Called
 * once per host frame from the cart's frame() after the scheduler runs. */
void cron_vid_present (void) {
    if (!s_inited || !s_screen[0]) return;
    const uint32_t *src = s_screen[0];
    for (int i = 0; i < VID_W * VID_H; ++i)
        s_fb8[i] = rgb332 (src[i]);
    cron_blit (s_fb8, VID_W, VID_H, 0, 0);
    cron_present ();
}
