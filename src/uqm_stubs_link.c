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
void LoadKernel () { /* link-only stub */; }
void SplashScreen () { /* link-only stub */; }
void StartGame () { /* link-only stub */; }
void SetPlayerInputAll () { /* link-only stub */; }
void InitGameStructures () { /* link-only stub */; }
void InitGameClock () { /* link-only stub */; }
void AddInitialGameEvents () { /* link-only stub */; }
void SetStatusMessageMode () { /* link-only stub */; }
void getGameState () { /* link-only stub */; }
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
void UninitGameStructures () { /* link-only stub */; }
void ClearPlayerInputAll () { /* link-only stub */; }
void UninitGameKernel () { /* link-only stub */; }
void FreeMasterShipList () { /* link-only stub */; }
void FreeKernel () { /* link-only stub */; }
void LoadMasterShipList () { /* link-only stub */; }
void InitGameKernel () { /* link-only stub */; }
void UpdateInputState () { /* link-only stub */; }
void GameClockTick () { /* link-only stub */; }
void setGameState () { /* link-only stub */; }
void SeedUniverse () { /* link-only stub */; }
