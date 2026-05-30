/*
 * uqm_stubs_link.c — link-surface stubs for UQM subsystems not yet compiled.
 *
 * Two kinds of stub, kept STRICTLY apart after the ARCTAN incident (a math
 * leaf mis-stubbed as "battle-only" returned garbage → a garbage planet
 * image.frame → DrawStamp drew a garbage-sized stamp = a silent 1.5-hour
 * infinite-loop hang):
 *
 *   1. DELIBERATE no-ops / return-0 — functions that ARE reached on the live
 *      path and degrade gracefully (audio with no backend, a stationary
 *      flagship, a behavioural seam). These do nothing (or return the one
 *      correct sentinel, e.g. NULL/0) and must NOT corrupt state.
 *
 *   2. STUB_TRAP — functions that should NEVER be reached on the current paths
 *      (boot, the menu, New Game → the interplanetary view). These belong to
 *      subsystems not yet brought up (battle/melee, planet-surface scan/orbit,
 *      the sub-menus / star map / communication). If one is ever called it is
 *      a MIS-CLASSIFICATION (the next ARCTAN): it LOGS ITS OWN NAME and traps
 *      immediately, so the bug is an instant, named halt — never silent
 *      garbage. When a subsystem lands, its STUB_TRAPs are deleted as the real
 *      TUs join build_uqm.sh's KEEP list.
 *
 * RULE learned: a value-returning function must NEVER be a silent stub. Either
 * compile its real owner (general math/util leaves — ARCTAN/trans.c,
 * square_root/sqrt.c, TFB_Random/random.c — are compiled, NOT stubbed) or
 * STUB_TRAP it. Before stubbing anything, grep its callers across ALL
 * subsystems; shared leaves are used everywhere.
 */
#include <stdint.h>
#include "port.h"
#include "libs/compiler.h"
#include "libs/log.h"

/* A STUB_TRAP logs its name then halts (llvm.trap → VM HALT). The headless
 * runner prints the trap; the log line right before it names the culprit. */
static void cron_stub_unreached (const char *name) {
    log_add (log_Fatal, "CRON: UNREACHED STUB CALLED (mis-classified — compile "
            "its owner or reclassify): %s", name);
    __builtin_trap ();
}
#define STUB_TRAP(fn)  void fn (void) { cron_stub_unreached (#fn); }

/* ====================================================================== *
 * 1. DELIBERATE no-ops / return-0 — reached on the live path, graceful.   *
 * ====================================================================== */

/* ---- audio / video: no backend yet, so every call is a silent no-op.
 *      These ARE invoked during normal play (menu sounds, music, SFX). ---- */
void initAudio () { }
void InitSound () { }
void UninitSound () { }
void StopSound () { }
void DestroySound () { }
void PlaySoundEffect () { }
void PlayMusic () { }
void StopMusic () { }
void DestroyMusic () { }
void InitVideoPlayer () { }
void UninitVideoPlayer () { }
void InstallAudioResTypes () { }
void InstallVideoResType () { }
void ProcessSound () { }
void PlaySound () { }
void CalcSoundPosition () { }
void SetPlanetMusic () { }
void NotPositional () { }
void FlushSounds () { }
void RemoveSoundsForObject () { }
void UpdateSoundPositions () { }
/* Value-returning audio handles: 0/NULL is the CORRECT sentinel (a captured
 * NULL handle makes the dependent op a guarded no-op — NOT garbage). */
void *LoadSoundInstance () { return 0; }      /* -> MenuSounds == NULL -> skipped */
void *LoadMusicInstance () { return 0; }      /* -> hMusic == NULL -> skipped */
uint32_t FadeMusic () { return 0; }           /* a past deadline -> SleepThreadUntil no-op */
int PLRPlaying () { return 0; }               /* nothing is ever playing */

/* ---- behavioural seams / graceful degradation (reached, no corruption) ---- */
/* SplashScreen: MUST invoke its callback (BackgroundInitKernel runs
 * LoadMasterShipList); we just skip the splash graphic. */
void SplashScreen (void (*DoProcessing)(DWORD TimeOut)) { if (DoProcessing) DoProcessing (0); }
/* The cron present is a per-frame full downsample; no transition upload step. */
void TFB_UploadTransitionScreen () { }
/* No addon packs. */
void loadAddon () { }
void prepareAddons () { }
/* Input-frame bookkeeping — harmless to skip. */
void BeginInputFrame () { }
void DoConfirmExit () { }
void TFB_ResetControls () { }
/* MoveSIS(SIZE*,SIZE*): the real one (hyper.c) moves the flagship by the input
 * delta; a no-op leaves the deltas untouched, so the flagship just sits still
 * — graceful for now (it does NOT return garbage). Compiling hyper.c is a
 * later slice. */
void MoveSIS () { }
/* Alarm timer lib not built — no scheduled alarms. */
void Alarm_addAbsoluteMs () { }
void Alarm_remove () { }
/* util.c (UI box / RNG seed / pause) not yet compiled; these no-op gracefully
 * (a missing box / unseeded-but-deterministic RNG / no pause). Void, no garbage. */
void DrawStarConBox () { }
void SeedRandomNumbers () { }
void PauseGame () { }
void SleepGame () { }
/* Battle/hyper DATA teardown — nothing was allocated (those subsystems are
 * stubbed), so freeing is a no-op. Void, no garbage. */
void FreeLanderData () { }
void FreeHyperData () { }
/* init.c space setup — InitSISContexts/the view ran fine with these no-op'd;
 * keep them graceful (they may sit on the view-init path). */
void InitSpace () { }
void UninitSpace () { }
void InitStatusOffsets () { }
/* LoadLanderData is called by InitSolarSys (solarsys.c) ON the view-init path,
 * not just on landing — no-op (no lander data) is graceful; void, no garbage. */
void LoadLanderData () { }
/* On the New-Game / IP-flight path, reached + graceful (void, no garbage). The
 * intro story, universe seeding (state already set by InitGameStructures) and
 * the per-tick hyperspace-encounter check no-op until those subsystems land. */
void Introduction () { }
void SeedUniverse () { }
void check_hyperspace_encounter () { }
/* DrawMenuStateStrings: the SIS panel draws the status-text bar every frame
 * (sis.c) — void, on the passive view path; no-op = no bottom text (graceful). */
void DrawMenuStateStrings () { }

/* ====================================================================== *
 * 2. STUB_TRAP — must never be reached on boot / menu / interplanetary.   *
 *    If hit, it is a mis-classification: instant named halt, not garbage. *
 * ====================================================================== */

/* ---- battle / melee combat engine (cyborg AI, weapon, ship physics,
 *      velocity, gravity, status deltas, tactical transitions). Reached only
 *      in an actual battle. Many return values → a void stub would be a
 *      garbage landmine, hence TRAP. ---- */
STUB_TRAP (ship_intelligence)
STUB_TRAP (ship_weapons)
STUB_TRAP (PlotIntercept)
STUB_TRAP (computer_intelligence)
STUB_TRAP (selectShipComputer)
STUB_TRAP (battleEndReadyComputer)
STUB_TRAP (frameInputHuman)
STUB_TRAP (selectShipHuman)
STUB_TRAP (battleEndReadyHuman)
STUB_TRAP (initialize_laser)
STUB_TRAP (initialize_missile)
STUB_TRAP (weapon_collision)
STUB_TRAP (ModifySilhouette)
STUB_TRAP (TrackShip)
STUB_TRAP (collision)
STUB_TRAP (inertial_thrust)
STUB_TRAP (CalculateGravity)
STUB_TRAP (TimeSpaceMatterConflict)
STUB_TRAP (DeltaCrew)
STUB_TRAP (DeltaEnergy)
STUB_TRAP (StartShipExplosion)
STUB_TRAP (FindAliveStarShip)
STUB_TRAP (GetWinnerStarShip)
STUB_TRAP (SetWinnerStarShip)
STUB_TRAP (RecordShipDeath)
STUB_TRAP (StopAllBattleMusic)
STUB_TRAP (AbandonShip)
STUB_TRAP (Battle)
STUB_TRAP (collide)
STUB_TRAP (do_damage)
STUB_TRAP (load_animation)   /* load_ship(LoadBattleData=TRUE) only — we pass FALSE */
STUB_TRAP (free_image)       /* free_ship(FreeBattleData=TRUE) only */

/* ---- planet-SURFACE engine + scan/lander/report: reached only when the
 *      player ORBITS or SCANS a world (deeper than the system view). ---- */
STUB_TRAP (DoDiscoveryReport)
STUB_TRAP (KillLanderCrewSeq)
STUB_TRAP (SetLanderTakeoff)

/* ---- sub-menus / star map / communication / leaf activities: reached only
 *      by a specific player action we have not enabled yet. ---- */
STUB_TRAP (CargoMenu)
STUB_TRAP (DevicesMenu)
STUB_TRAP (RosterMenu)
STUB_TRAP (StarMap)
STUB_TRAP (MoveGalaxy)
STUB_TRAP (DoMenuChooser)
STUB_TRAP (GameOptions)
STUB_TRAP (VisitStarBase)
STUB_TRAP (RaceCommunication)
STUB_TRAP (InitCommunication)
STUB_TRAP (SetupMenu)
STUB_TRAP (DoPopupWindow)
STUB_TRAP (Melee)
STUB_TRAP (Victory)
STUB_TRAP (Credits)
STUB_TRAP (InstallBombAtEarth)
STUB_TRAP (HyperspaceMenu)

/* ---- planet-surface engine landed (pl_stuff/plangen/gentopo/surface/scan +
 *      cons_res). New boundary it exposes: ---- */
void PlayMenuSound () { }            /* audio no-op (sounds.c) */
/* WaitForAnyButtonUntil(newButton,timeOut,...) -> BOOLEAN: 0 = "timed out, no
 * button" is the correct sentinel (the scan screen auto-advances), not garbage. */
int WaitForAnyButtonUntil () { return 0; }
/* lander.c — the LANDING minigame (deeper than orbit/scan); not reached by the
 * scan path, so STUB_TRAP (loud if a descent is ever triggered). */
STUB_TRAP (InitLander)
STUB_TRAP (PlanetSide)
STUB_TRAP (object_animation)
