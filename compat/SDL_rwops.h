/*
 * compat/SDL_rwops.h -- minimal SDL_RWops stand-in for the Cronopio UQM port.
 *
 * Only sdluio.h references SDL_RWops (the SDL_image file-IO adapter). We do
 * not use SDL_image (content images are decoded cart-side), so SDL_RWops is
 * an opaque placeholder just to satisfy the prototypes in sdluio.h. The real
 * image loader (TFB_DrawCanvas_LoadFromFile) is stubbed in sdl_compat.c until
 * the cel/PNG decode slice lands.
 */
#ifndef CRON_COMPAT_SDL_RWOPS_H
#define CRON_COMPAT_SDL_RWOPS_H

#include "SDL.h"

typedef struct SDL_RWops SDL_RWops;

#endif /* CRON_COMPAT_SDL_RWOPS_H */
