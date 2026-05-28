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
/* LoadKernel returns BOOLEAN — fake TRUE so Starcon2Main proceeds past
 * its content-missing fatal. The real impl loads content packs +
 * inits sound/gfx subsystems; we just need it to "succeed" so we can
 * see what fails NEXT. eax-trick: return value is in R0 per ABI; emit
 * a tiny inline asm-free body that sets it to 1 via a typed return. */
int LoadKernel (int kernel, int gfxdriver) { (void)kernel; (void)gfxdriver; return 1; }
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
void SetPlayerInputAll () { /* link-only stub */; }
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
void ClearPlayerInputAll () { /* link-only stub */; }
void UninitGameKernel () { /* link-only stub */; }
/* FreeMasterShipList — now real, in uqm/master.c (KEEP) */
void FreeKernel () { /* link-only stub — real in uqm/cleanup.c (not in KEEP yet) */ }
/* LoadMasterShipList — now real, in uqm/master.c (KEEP) */
void InitGameKernel () { /* link-only stub */; }
void UpdateInputState () { /* link-only stub */; }
void GameClockTick () { /* link-only stub */; }
/* setGameState — now real, in uqm/globdata.c (KEEP) */
void SeedUniverse () { /* link-only stub */; }
void LoadGraphicInstance () { /* link-only stub */ }
void CaptureDrawable () { /* link-only stub */ }
void CreateContextAux () { /* link-only stub */ }
void SetContext () { /* link-only stub */ }
void SetContextFGFrame () { /* link-only stub */ }
void SetContextClipRect () { /* link-only stub */ }
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
void SetAbsStringTableIndex () { /* link-only stub */ }
void GetStringAddress () { /* link-only stub */ }
void utf8StringCopy () { /* link-only stub */ }
void SetRaceAllied () { /* link-only stub */ }
void CloneShipFragment () { /* link-only stub */ }
void DestroyContext () { /* link-only stub */ }
void ReleaseDrawable () { /* link-only stub */ }
void DestroyDrawable () { /* link-only stub */ }
/* UninitQueue — now real, in uqm/displist.c (KEEP) */
void UninitGroupInfo () { /* link-only stub */ }
void UninitPlanetInfo () { /* link-only stub */ }
void ReleaseStringTable () { /* link-only stub */ }
void DestroyStringTable () { /* link-only stub */ }
void GetStarShipFromIndex () { /* link-only stub */ }
