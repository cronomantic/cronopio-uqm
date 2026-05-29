/*
 * uqm_stubs_link.c — link-surface stubs for UQM subsystems not yet compiled.
 * Each function is a no-op (or a minimal behavioural seam); as the real owning
 * .c files land in build_uqm.sh's KEEP lists, the corresponding stubs come out.
 *
 * GFX slice 2 (this round): the TFB drawing layer is now REAL — the
 * platform-independent front-end (tfb_draw.c / tfb_prim.c / dcqueue.c / bbox.c)
 * + the stock SDL backend (sdl/canvas.c / primitives.c / rotozoom.c / palette.c)
 * compile against the SDL_Surface shim (compat/SDL.h + src/sdl_compat.c). So all
 * the TFB_DrawScreen_* / TFB_DrawImage_* / TFB_DrawCanvas_* / TFB_Prim_* /
 * TFB_Batch* / Native-palette stubs are GONE (now real). What remains stubbed:
 * the cel/font/graphic-resource loaders (gfxload/font/resgfx/loaddisp/intersec),
 * audio, input, and the game-init / gameplay subsystems.
 */
#include <stdint.h>
#include "port.h"
#include "libs/compiler.h"

/* ---- audio / video player (not compiled) ---- */
void initAudio () { /* link-only stub */; }
void StopSound () { /* link-only stub */; }
void InitSound () { /* link-only stub */ }
void InitVideoPlayer () { /* link-only stub */ }
void LoadSoundInstance () { /* link-only stub */ }

/* ---- input (not compiled yet — input seam is the next slice) ---- */
void FlushInput () { /* link-only stub */; }
void UpdateInputState () { /* link-only stub */; }
void HumanInputContext_new () { /* link-only stub */ }
void ComputerInputContext_new () { /* link-only stub */ }

/* SplashScreen(cb): the real one shows splash gfx then invokes the callback
 * (BackgroundInitKernel), which runs LoadMasterShipList + InitGameKernel. We
 * don't draw the splash yet, but we MUST invoke the callback or the master
 * ship list never initialises and LockMasterShip asserts on an empty
 * master_q. Behavioural seam stub, not a no-op. */
void SplashScreen (void (*DoProcessing)(DWORD TimeOut)) {
    if (DoProcessing) DoProcessing (0);
}
/* StartGame: returns BOOLEAN (start a new game?). No menu/UI yet, so return
 * FALSE — Starcon2Main's `while (StartGame())` loop exits cleanly instead of
 * spinning forever on a garbage return value. */
int StartGame () { return 0; }

/* ---- game clock / init / gameplay subsystems (not compiled yet) ---- */
void InitGameClock () { /* link-only stub */; }
void UninitGameClock () { /* link-only stub */; }
void SetGameClockRate () { /* link-only stub */; }
void GameClockTick () { /* link-only stub */; }
void AddInitialGameEvents () { /* link-only stub */; }
void SetStatusMessageMode () { /* link-only stub */; }
void InstallBombAtEarth () { /* link-only stub */; }
void VisitStarBase () { /* link-only stub */; }
void RaceCommunication () { /* link-only stub */; }
void InitCommunication () { /* link-only stub */; }
void DrawAutoPilotMessage () { /* link-only stub */; }
void ExploreSolarSys () { /* link-only stub */; }
void Battle () { /* link-only stub */; }
void SetFlashRect () { /* link-only stub */; }
void SeedUniverse () { /* link-only stub */; }
void UninitGameKernel () { /* link-only stub */; }
void FreeKernel () { /* link-only stub — real in uqm/cleanup.c (not in KEEP yet) */ }
void InitSISContexts () { /* link-only stub */ }
void InitPlanetInfo () { /* link-only stub */ }
void InitGroupInfo () { /* link-only stub */ }
void UninitGroupInfo () { /* link-only stub */ }
void UninitPlanetInfo () { /* link-only stub */ }
void SetRaceAllied () { /* link-only stub */ }
void CloneShipFragment () { /* link-only stub */ }
void GetStarShipFromIndex () { /* link-only stub */ }
void InitStatusOffsets () { /* link-only stub */ }
void InitSpace () { /* link-only stub */ }
/* load_ship: no ship content packs yet → return NULL so LoadMasterShipList
 * cleanly skips each entry (builds an empty master_q) instead of dereferencing
 * a garbage RACE_DESC*. */
void *load_ship () { return 0; }
void free_ship () { /* link-only stub */ }

/* ---- graphic / cel / font resource loaders (gfxload/font/resgfx/loaddisp/
 *      intersec — not compiled yet; the cel + font decode is the slice after
 *      the backend) ---- */
void LoadGraphicInstance () { /* link-only stub */ }
void InstallGraphicResTypes () { /* link-only stub */ }
void InstallAudioResTypes () { /* link-only stub */ }
void InstallVideoResType () { /* link-only stub */ }
void InstallCodeResType () { /* link-only stub */ }
void LoadDisplayPixmap () { /* link-only stub */ }
void _ReleaseCelData () { /* link-only stub */ }
void GetContextFontLeading () { /* link-only stub */ }
void GetContextFontLeadingWidth () { /* link-only stub */ }
void TextRect () { /* link-only stub */ }
void _text_blt () { /* link-only stub */ }
void BoxIntersect () { /* link-only stub */ }

/* ---- addons (not compiled) ---- */
void loadAddon () { /* link-only stub */ }
void prepareAddons () { /* link-only stub */ }

/* TFB_UploadTransitionScreen: normally dispatches through the SDL graphics
 * backend vtable (graphics_backend->uploadTransitionScreen). The cron backend
 * has no transition-upload step (the present is a per-frame full downsample),
 * so this is a no-op. */
void TFB_UploadTransitionScreen () { /* link-only stub */ }

/* sdluio_loadImage: SDL_image-backed PNG loader for cel/font graphics. We do
 * not use SDL_image — content image decode (PNG -> 8bpp/32bpp canvas) is a
 * later slice — so loading from file fails gracefully for now. Callers
 * (TFB_DrawCanvas_LoadFromFile) handle a NULL return. */
void *sdluio_loadImage (void *dir, const char *fileName) {
    (void) dir; (void) fileName;
    return 0;
}
