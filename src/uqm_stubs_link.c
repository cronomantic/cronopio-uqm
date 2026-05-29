/*
 * uqm_stubs_link.c — auto-discovered link-surface stubs from
 * tools/survey_link.sh. Each function abort-traps on call; the goal is
 * to enumerate WHICH symbols starcon.c reaches, not to do anything
 * useful. As the real owning .c files land in build_uqm.sh's KEEP list,
 * the corresponding stubs come out.
 */
#include <stdint.h>
#include "port.h"
#include "libs/compiler.h"

void FlushInput () { /* link-only stub */; }
void TFB_DrawScreen_ReinitVideo () { /* link-only stub */; }
void initAudio () { /* link-only stub */; }
/* LoadKernel / InitGameKernel / InitContexts / SetPlayerInputAll /
 * ClearPlayerInputAll / InitPlayerInput / initIO — now REAL, in uqm/setup.c
 * (the forced-TRUE LoadKernel stub is gone; the real one loads the colormap
 * + creates the screen context from mounted content). */
/* SplashScreen(cb): the real one shows splash gfx then invokes the
 * callback (BackgroundInitKernel) which runs LoadMasterShipList +
 * InitGameKernel. We don't draw the splash yet, but we MUST invoke the
 * callback or the master ship list never initialises and LockMasterShip
 * asserts on an empty master_q. Behavioural seam stub, not a no-op. */
void SplashScreen (void (*DoProcessing)(DWORD TimeOut)) {
    if (DoProcessing) DoProcessing (0);
}
/* StartGame: returns BOOLEAN (start a new game?). No menu/UI yet, so
 * return FALSE — Starcon2Main's `while (StartGame())` loop exits cleanly
 * instead of spinning forever on a garbage return value. */
int StartGame () { return 0; }
/* SetPlayerInputAll — now real, uqm/setup.c */
/* InitGameStructures — now real, in uqm/globdata.c (KEEP) */
void InitGameClock () { /* link-only stub */; }
void AddInitialGameEvents () { /* link-only stub */; }
void SetStatusMessageMode () { /* link-only stub */; }
/* getGameState — now real, in uqm/globdata.c (KEEP) */
void InstallBombAtEarth () { /* link-only stub */; }
void VisitStarBase () { /* link-only stub */; }
void RaceCommunication () { /* link-only stub */; }
void DrawAutoPilotMessage () { /* link-only stub */; }
void SetGameClockRate () { /* link-only stub */; }
void ExploreSolarSys () { /* link-only stub */; }
void Battle () { /* link-only stub */; }
void SetFlashRect () { /* link-only stub */; }
void InitCommunication () { /* link-only stub */; }
void StopSound () { /* link-only stub */; }
void UninitGameClock () { /* link-only stub */; }
/* UninitGameStructures — now real, in uqm/globdata.c (KEEP) */
/* ClearPlayerInputAll — now real, uqm/setup.c */
void UninitGameKernel () { /* link-only stub */; }
/* FreeMasterShipList — now real, in uqm/master.c (KEEP) */
void FreeKernel () { /* link-only stub — real in uqm/cleanup.c (not in KEEP yet) */ }
/* LoadMasterShipList — now real, in uqm/master.c (KEEP) */
/* InitGameKernel — now real, uqm/setup.c */
void UpdateInputState () { /* link-only stub */; }
void GameClockTick () { /* link-only stub */; }
/* setGameState — now real, in uqm/globdata.c (KEEP) */
void SeedUniverse () { /* link-only stub */; }
void LoadGraphicInstance () { /* link-only stub */ }
/* CaptureDrawable — now real, in libs/graphics/pixmap.c
 * CreateContextAux / SetContext / SetContextClipRect / DestroyContext —
 * now real, in libs/graphics/context.c */
/* InitQueue — now real, in uqm/displist.c (KEEP) */
/* AllocLink — now real, in uqm/displist.c (KEEP) */
/* FindMasterShip / FindMasterShipIndex / GetShipCostFromIndex /
 * GetShipIconsFromIndex / GetShipMeleeIconsFromIndex — now real,
 * in uqm/master.c (KEEP) */
/* load_ship: no content packs yet → return NULL so LoadMasterShipList
 * cleanly skips each entry (builds an empty master_q) instead of
 * dereferencing a garbage RACE_DESC*. */
void *load_ship () { return 0; }
void free_ship () { /* link-only stub */ }
/* PutQueue — now real, in uqm/displist.c (KEEP) */
void InitSISContexts () { /* link-only stub */ }
void InitPlanetInfo () { /* link-only stub */ }
void InitGroupInfo () { /* link-only stub */ }
/* SetAbsStringTableIndex / GetStringAddress — now real, libs/strings/strings.c
 * utf8StringCopy — now real, libs/strings/unicode.c */
void SetRaceAllied () { /* link-only stub */ }
void CloneShipFragment () { /* link-only stub */ }
/* ReleaseDrawable — now real, in libs/graphics/pixmap.c */
/* SetContextFGFrame / DestroyDrawable — now real, in libs/graphics/{context,drawable}.c */
/* UninitQueue — now real, in uqm/displist.c (KEEP) */
void UninitGroupInfo () { /* link-only stub */ }
void UninitPlanetInfo () { /* link-only stub */ }
/* ReleaseStringTable / DestroyStringTable — now real, libs/strings/strings.c */
void GetStarShipFromIndex () { /* link-only stub */ }
/* AllocStringTable / FreeStringTable — now real, in libs/strings/strings.c */
void InstallGraphicResTypes () { /* link-only stub */ }
/* InstallStringTableResType — now real, in libs/strings/sresins.c */
void InstallAudioResTypes () { /* link-only stub */ }
void InstallVideoResType () { /* link-only stub */ }
void InstallCodeResType () { /* link-only stub */ }
void TFB_DrawScreen_Copy () { /* link-only stub */ }
void TFB_BatchGraphics () { /* link-only stub */ }
void TFB_UnbatchGraphics () { /* link-only stub */ }
void TFB_DrawScreen_WaitForSignal () { /* link-only stub */ }
void TFB_UploadTransitionScreen () { /* link-only stub */ }
void TFB_DrawImage_Delete () { /* link-only stub */ }
void GetContextFontLeading () { /* link-only stub */ }
void GetContextFontLeadingWidth () { /* link-only stub */ }
void TFB_DrawScreen_DeleteImage () { /* link-only stub */ }
void TFB_DrawImage_CreateForScreen () { /* link-only stub */ }
void TFB_DrawImage_Image () { /* link-only stub */ }
void TFB_DrawImage_Rect () { /* link-only stub */ }
void BoxIntersect () { /* link-only stub */ }
void LoadDisplayPixmap () { /* link-only stub */ }
void _ReleaseCelData () { /* link-only stub */ }
void TFB_DrawCanvas_New_TrueColor () { /* link-only stub */ }
void TFB_DrawImage_New () { /* link-only stub */ }
void TFB_DrawImage_New_Rotated () { /* link-only stub */ }
void TFB_DrawCanvas_SetTransparentColor () { /* link-only stub */ }
void TFB_DrawCanvas_GetPixel () { /* link-only stub */ }
void TFB_DrawImage_CopyRect () { /* link-only stub */ }
/* GetFrameParentDrawable — now real, in libs/graphics/pixmap.c */
void TFB_DrawCanvas_Rescale_Nearest () { /* link-only stub */ }
void TFB_DrawCanvas_GetPixelColors () { /* link-only stub */ }
void TFB_DrawCanvas_SetPixelColors () { /* link-only stub */ }
void TFB_DrawCanvas_GetPixelIndexes () { /* link-only stub */ }
void TFB_DrawCanvas_SetPixelIndexes () { /* link-only stub */ }
void TFB_Prim_FillRect () { /* link-only stub */ }
void TFB_Prim_Point () { /* link-only stub */ }
void TFB_Prim_Stamp () { /* link-only stub */ }
void TFB_Prim_StampFill () { /* link-only stub */ }
void TFB_Prim_Line () { /* link-only stub */ }
void TextRect () { /* link-only stub */ }
void _text_blt () { /* link-only stub */ }
void TFB_Prim_Rect () { /* link-only stub */ }
void FreeNativePalette () { /* link-only stub */ }
void GetNativePaletteColor () { /* link-only stub */ }
void SetNativePaletteColor () { /* link-only stub */ }
void AllocNativePalette () { /* link-only stub */ }
void InitSound () { /* link-only stub */ }
void InitVideoPlayer () { /* link-only stub */ }
void loadAddon () { /* link-only stub */ }
void prepareAddons () { /* link-only stub */ }
void LoadSoundInstance () { /* link-only stub */ }
void InitStatusOffsets () { /* link-only stub */ }
void InitSpace () { /* link-only stub */ }
void HumanInputContext_new () { /* link-only stub */ }
void ComputerInputContext_new () { /* link-only stub */ }
