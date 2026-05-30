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
/* LoadSound = LoadSoundInstance (nameref.h); the result feeds CaptureSound
 * (= CaptureStringTable) -> MenuSounds. Audio isn't implemented, so return 0
 * (NULL): CaptureStringTable(0) -> NULL -> MenuSounds == 0, so DoInput's
 * menu-sound block is skipped. A void stub returned a GARBAGE non-NULL handle
 * -> MenuSounds invalid -> SetAbsSoundIndex(MenuSounds) (= SetAbsStringTableIndex)
 * OOB-trapped on every menu SELECT. */
void *LoadSoundInstance () { return 0; }

/* ---- input — FlushInput/UpdateInputState now REAL (uqm/gameinp.c);
 *      Human/ComputerInputContext_new + PlayerInput[] now REAL
 *      (uqm/battlecontrols.c). ---- */
/* battlecontrols.c's Human/ComputerInputHandlers tables initialise their
 * frameInput/selectShip/battleEndReady members with these battle/AI handlers.
 * The translator needs definitions to resolve the function-pointer
 * initialisers, but they're only CALLED during battle (slice 5b). */
void computer_intelligence () { /* link-only stub */ }
void selectShipComputer () { /* link-only stub */ }
void battleEndReadyComputer () { /* link-only stub */ }
void frameInputHuman () { /* link-only stub */ }
void selectShipHuman () { /* link-only stub */ }
void battleEndReadyHuman () { /* link-only stub */ }

/* SplashScreen(cb): the real one shows splash gfx then invokes the callback
 * (BackgroundInitKernel), which runs LoadMasterShipList + InitGameKernel. We
 * don't draw the splash yet, but we MUST invoke the callback or the master
 * ship list never initialises and LockMasterShip asserts on an empty
 * master_q. Behavioural seam stub, not a no-op. */
void SplashScreen (void (*DoProcessing)(DWORD TimeOut)) {
    if (DoProcessing) DoProcessing (0);
}
/* StartGame: now REAL — uqm/restart.c (the main menu state machine). */

/* ---- game clock / init / gameplay subsystems ----
 * Slice 5a landed the NEW-GAME init chain: clock.c (InitGameClock/
 * UninitGameClock/SetGameClockRate/GameClockTick), gameev.c
 * (AddInitialGameEvents), cleanup.c (UninitGameKernel/FreeKernel/FreeGameData),
 * border.c (InitSISContexts), state.c (Init/UninitPlanetInfo), grpinfo.c
 * (Init/UninitGroupInfo), build.c (SetRaceAllied/CloneShipFragment/
 * GetStarShipFromIndex), loadship.c (load_ship/free_ship). Those stubs are
 * gone. The gameplay activities below (ExploreSolarSys/Battle/VisitStarBase/
 * communication/...) stay stubbed — slice 5b. */
void SetStatusMessageMode () { /* link-only stub */; }
void InstallBombAtEarth () { /* link-only stub */; }
void VisitStarBase () { /* link-only stub */; }
void RaceCommunication () { /* link-only stub */; }
void InitCommunication () { /* link-only stub */; }
void DrawAutoPilotMessage () { /* link-only stub */; }
/* ExploreSolarSys — behavioural stub in src/uqm_seam.c (needs GLOBAL/CHECK_ABORT
 * from globdata.h): aborts the game loop so a NEW GAME runs the full init +
 * teardown cycle and returns to the menu instead of spinning forever on a
 * no-op (slice 5a). Real solar-system view is slice 5b. */
void Battle () { /* link-only stub */; }
void SetFlashRect () { /* link-only stub */; }
void SeedUniverse () { /* link-only stub */; }
void InitStatusOffsets () { /* link-only stub */ }
void InitSpace () { /* link-only stub */ }

/* ---- graphic / cel / font resource loaders — now REAL (resgfx.c / gfxload.c
 *      / font.c / loaddisp.c / filegfx.c compiled). So InstallGraphicResTypes,
 *      LoadGraphicInstance, _Get/ReleaseCelData, _Get/ReleaseFontData,
 *      _text_blt, TextRect, GetContextFontLeading[Width], LoadDisplayPixmap are
 *      gone. Audio/video/code resource types still stubbed. ---- */
void InstallAudioResTypes () { /* link-only stub */ }
void InstallVideoResType () { /* link-only stub */ }
/* InstallCodeResType / LoadCodeResInstance / CaptureCodeRes / ReleaseCodeRes /
 * DestroyCodeRes are now REAL — uqm/dummy.c (the "SHIP" code-resource type
 * handler). It registers the resource type whose loader maps a ship id ->
 * init_<ship>() so LoadMasterShipList can build master_q (slice 5b step 1). */
void BoxIntersect () { /* link-only stub — intersec.c not compiled */ }

/* ---- addons (not compiled) ---- */
void loadAddon () { /* link-only stub */ }
void prepareAddons () { /* link-only stub */ }

/* TFB_UploadTransitionScreen: normally dispatches through the SDL graphics
 * backend vtable (graphics_backend->uploadTransitionScreen). The cron backend
 * has no transition-upload step (the present is a per-frame full downsample),
 * so this is a no-op. */
void TFB_UploadTransitionScreen () { /* link-only stub */ }

/* sdluio_loadImage is now REAL — src/img_cron.c (PNG decode via stb_image,
 * reading through uio). */
void SleepGame () { /* link-only stub */ }
void PauseGame () { /* link-only stub */ }
void BeginInputFrame () { /* link-only stub */ }
void DoConfirmExit () { /* link-only stub */ }
void TFB_ResetControls () { /* link-only stub */ }
void NotPositional () { /* link-only stub */ }
void PlaySoundEffect () { /* link-only stub */ }
void Victory () { /* link-only stub */ }
void Credits () { /* link-only stub */ }
void StopMusic () { /* link-only stub */ }
void DestroyMusic () { /* link-only stub */ }
void SeedRandomNumbers () { /* link-only stub */ }
void Melee () { /* link-only stub */ }
void Introduction () { /* link-only stub */ }
void *LoadMusicInstance () { return 0; }  /* no audio yet -> hMusic = 0 (skipped) */
void PlayMusic () { /* link-only stub */ }
void SetupMenu () { /* link-only stub */ }
void DoPopupWindow () { /* link-only stub */ }
/* FadeMusic returns a TimeCount (callers do SleepThreadUntil(FadeMusic(...)));
 * return 0 (a past deadline) so the sleep is a no-op. */
uint32_t FadeMusic () { return 0; }
void DrawStatusMessage () { /* link-only stub */ }
void check_hyperspace_encounter () { /* link-only stub */ }
void square_root () { /* link-only stub */ }
void UninitVideoPlayer () { /* link-only stub */ }
void UninitSound () { /* link-only stub */ }
void FreeLanderData () { /* link-only stub */ }
void FreeIPData () { /* link-only stub */ }
void FreeHyperData () { /* link-only stub */ }
void UninitSpace () { /* link-only stub */ }
void DestroySound () { /* link-only stub */ }
void DrawStarConBox () { /* link-only stub */ }
void ClearSISRect () { /* link-only stub */ }
void TFB_Random () { /* link-only stub */ }
void planetOuterLocation () { /* link-only stub */ }
void DeltaSISGauges () { /* link-only stub */ }
void load_animation () { /* link-only stub */ }
void free_image () { /* link-only stub */ }

/* ====================================================================== *
 * BATTLE-SUBSYSTEM BOUNDARY (slice 5b step 1).                            *
 *                                                                        *
 * The 28 ship TUs (uqm/ships/*) + dummy.c are now compiled so that       *
 * LoadMasterShipList can build master_q (load_ship -> LoadCodeRes ->     *
 * init_<ship> -> the ship's static RACE_DESC). Each ship's RACE_DESC is  *
 * loaded with LoadBattleData=FALSE, so only init_<ship> + its icon/melee/*
 * race-string resources are exercised at load time.                      *
 *                                                                        *
 * Every ship file ALSO defines its battle AI / preprocess / weapon-init  *
 * functions, which reference the COMBAT engine below (cyborg=AI,         *
 * weapon, process=ELEMENT system, tactrans=tactical transitions,         *
 * gravity, velocity, status, ship, hyper, sounds, trans, misc). Those    *
 * combat functions are ONLY invoked during battle frames — NEVER while   *
 * building the master ship list. They are stubbed here as the boundary   *
 * of the battle/melee subsystem, which is not yet brought up. When that  *
 * subsystem lands (its owning TUs added to KEEP), these stubs come out.  *
 * ====================================================================== */
/* process.c — the ELEMENT system */
void AllocElement () { /* battle-boundary stub */ }
void FreeElement () { /* battle-boundary stub */ }
void RemoveElement () { /* battle-boundary stub */ }
void Untarget () { /* battle-boundary stub */ }
/* tactrans.c — tactical transitions / battle outcome */
void StartShipExplosion () { /* battle-boundary stub */ }
void FindAliveStarShip () { /* battle-boundary stub */ }
void GetWinnerStarShip () { /* battle-boundary stub */ }
void SetWinnerStarShip () { /* battle-boundary stub */ }
void RecordShipDeath () { /* battle-boundary stub */ }
void StopAllBattleMusic () { /* battle-boundary stub */ }
/* weapon.c — projectile / laser init + collision */
void initialize_laser () { /* battle-boundary stub */ }
void initialize_missile () { /* battle-boundary stub */ }
void weapon_collision () { /* battle-boundary stub */ }
void ModifySilhouette () { /* battle-boundary stub */ }
void TrackShip () { /* battle-boundary stub */ }
/* cyborg.c — ship AI */
void ship_intelligence () { /* battle-boundary stub */ }
void ship_weapons () { /* battle-boundary stub */ }
void PlotIntercept () { /* battle-boundary stub */ }
/* ship.c — per-ship physics */
void collision () { /* battle-boundary stub */ }
void inertial_thrust () { /* battle-boundary stub */ }
/* velocity.c — velocity vector math */
void DeltaVelocityComponents () { /* battle-boundary stub */ }
void GetCurrentVelocityComponents () { /* battle-boundary stub */ }
void GetNextVelocityComponents () { /* battle-boundary stub */ }
void SetVelocityComponents () { /* battle-boundary stub */ }
void SetVelocityVector () { /* battle-boundary stub */ }
/* gravity.c — gravity / matter conflict */
void CalculateGravity () { /* battle-boundary stub */ }
void TimeSpaceMatterConflict () { /* battle-boundary stub */ }
/* status.c — combat crew/energy deltas */
void DeltaCrew () { /* battle-boundary stub */ }
void DeltaEnergy () { /* battle-boundary stub */ }
/* sounds.c — battle audio (also the audio platform seam, not yet built) */
void ProcessSound () { /* battle-boundary stub */ }
void PlaySound () { /* battle-boundary stub */ }
void CalcSoundPosition () { /* battle-boundary stub */ }
/* misc.c / hyper.c / trans.c — battle-reachable leaves */
void AbandonShip () { /* battle-boundary stub */ }
void HyperspaceMenu () { /* battle-boundary stub */ }
void ARCTAN () { /* battle-boundary stub */ }
