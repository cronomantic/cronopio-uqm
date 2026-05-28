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
void SplashScreen () { /* link-only stub */; }
void StartGame () { /* link-only stub */; }
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
void FreeMasterShipList () { /* link-only stub */; }
void FreeKernel () { /* link-only stub — real in uqm/cleanup.c (not in KEEP yet) */ }
void LoadMasterShipList () { /* link-only stub */; }
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
void FindMasterShip () { /* link-only stub */ }
void load_ship () { /* link-only stub */ }
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
