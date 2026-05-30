/*
 * main_cron.c — cart entry for the UQM threading spike.
 *
 * Goal: prove the cooperative-coroutine threading layer works end-to-end
 * by running a stub Starcon2Main and TWO worker threads cooperatively
 * through one cron_set_frame() callback. If we see all three threads
 * make progress (counters tick) without deadlock, the spike succeeds.
 *
 * Once that works, the next session replaces the stub Starcon2Main with
 * the real one (src/uqm/starcon.c) and starts compiling the rest of UQM.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <cronopio.h>

#include "port.h"
#include "libs/threadlib.h"
#include "libs/log.h"
#include "libs/timelib.h"
#include "libs/uio.h"
#include "libs/uio/fstypes.h"
#include "libs/reslib.h"
#include "libs/gfxlib.h"
#include "uqm/controls.h"     /* KEY_MENU_*, CONTROLLER_INPUT_STATE, Pulsed/ImmediateInputState */
#include "vid_cron.h"

/* ---- input seam: 12-button pad -> UQM's real input pipeline (gameinp.c) ----
 * Each frame we mirror the pad into ImmediateInputState.menu[] and call the
 * real UpdateInputState(), which derives Current/PulsedInputState with UQM's
 * pulse/key-repeat logic. The menu then reads the REAL PulsedInputState. */
extern volatile CONTROLLER_INPUT_STATE ImmediateInputState;
extern CONTROLLER_INPUT_STATE PulsedInputState;
extern void UpdateInputState (void);
extern void SetDefaultMenuRepeatDelay (void);
extern volatile int GameActive;
extern uint32_t cron_pad (int player);

static void cron_input_init (void) {
    GameActive = 1;               /* else UpdateInputState calls SleepGame */
    SetDefaultMenuRepeatDelay ();  /* init the pulse/repeat accel constants */
}

/* Mirror the 12-button pad into UQM's raw ImmediateInputState each frame. The
 * derivation to Current/PulsedInputState is done by UQM's own UpdateInputState
 * (called from inside DoInput's loop) -- we must NOT call it here too, or the
 * pulse/repeat timing double-steps. Feed BEFORE the scheduler runs so DoInput
 * sees fresh input this frame. */
static void cron_input_poll (void) {
    uint32_t p = cron_pad (0);
    volatile int *m = ImmediateInputState.menu;
    /* d-pad -> menu directions */
    m[KEY_MENU_UP]        = (p & (1u << 0)) ? 1 : 0;  /* CRON_BTN_UP    */
    m[KEY_MENU_DOWN]      = (p & (1u << 1)) ? 1 : 0;  /* CRON_BTN_DOWN  */
    m[KEY_MENU_LEFT]      = (p & (1u << 2)) ? 1 : 0;  /* CRON_BTN_LEFT  */
    m[KEY_MENU_RIGHT]     = (p & (1u << 3)) ? 1 : 0;  /* CRON_BTN_RIGHT */
    /* A or START -> select; B -> cancel; L/R -> page up/down */
    m[KEY_MENU_SELECT]    = (p & ((1u << 4) | (1u << 10))) ? 1 : 0; /* A / START */
    m[KEY_MENU_CANCEL]    = (p & (1u << 5)) ? 1 : 0;  /* CRON_BTN_B */
    m[KEY_MENU_PAGE_UP]   = (p & (1u << 8)) ? 1 : 0;  /* CRON_BTN_L */
    m[KEY_MENU_PAGE_DOWN] = (p & (1u << 9)) ? 1 : 0;  /* CRON_BTN_R */

    /* GAMEPLAY (flight/battle) controls. Menu nav reads menu[] above, but the
     * interplanetary SIS flight + battle read CurrentInputState.key[<player
     * template>][KEY_*] (e.g. solarsys.c: key[..][KEY_UP]=thrust, KEY_LEFT/RIGHT
     * =turn). UpdateInputState copies ImmediateInputState wholesale into
     * CurrentInputState, so filling key[] here drives the ship. PlayerControls[]
     * picks the template per player (default 0); fill ALL NUM_TEMPLATES so any
     * binding works. d-pad = thrust/turn, A=weapon, B=special. */
    {
        int up = (p & (1u << 0)) ? 1 : 0, down = (p & (1u << 1)) ? 1 : 0;
        int left = (p & (1u << 2)) ? 1 : 0, right = (p & (1u << 3)) ? 1 : 0;
        int wpn = (p & (1u << 4)) ? 1 : 0, spc = (p & (1u << 5)) ? 1 : 0;
        for (int t = 0; t < NUM_TEMPLATES; ++t) {
            ImmediateInputState.key[t][KEY_UP]      = up;     /* thrust */
            ImmediateInputState.key[t][KEY_DOWN]    = down;
            ImmediateInputState.key[t][KEY_LEFT]    = left;   /* turn left */
            ImmediateInputState.key[t][KEY_RIGHT]   = right;  /* turn right */
            ImmediateInputState.key[t][KEY_WEAPON]  = wpn;    /* A */
            ImmediateInputState.key[t][KEY_SPECIAL] = spc;    /* B */
        }
    }
}

/* Cron backend's scheduler entry — declared in
 * sc2/src/libs/threads/cron/cronthreads.h, mirrored here to avoid pulling
 * the backend header (which we want to remain a UQM-only file). */
extern uint32_t Scheduler_CRON_RunFrame (uint32_t tick_budget_ms);

/* Drains pendingBirth/pendingDeath in thrcommon.c — must be called from the
 * main "thread" each frame for StartThread() requests to actually spawn. */
extern void ProcessThreadLifecycles (void);

/* TFB draw-command queue drain (libs/graphics/dcqueue.c). UQM threads enqueue
 * draw commands; the "main thread" — here the host frame — drains them onto the
 * 32bpp screen canvas. Its empty-queue path calls TaskSwitch(), which is a
 * safe no-op from the host context (cronthreads guards s_current == NULL). */
extern void TFB_FlushGraphics (void);

/* GFX slice-2a backend self-test (src/sdl_compat.c): draws a pattern onto the
 * MAIN screen through the real TFB_DrawCanvas_* path, proving the 32bpp canvas
 * rendering reaches the framebuffer. */
extern void cron_gfx_selftest (void);
static int s_did_selftest;

/* res_GetResource by name — proves the GFXRES/FONTRES handlers decode real
 * content. Runs late (frame ~20) so InitKernel's InstallGraphicResTypes has
 * run. RESOURCE is a resource-name string. */
extern void *res_GetResource (const char *res);
extern const char *res_GetResourceType (const char *res);
static int s_frame_no;
static int s_did_res_selftest;

/* The real Starcon2Main lives in sc2/src/uqm/starcon.c (now in the build).
 * Declare the prototype so we can pass it to StartThread. */
extern int Starcon2Main (void *threadArg);

/* ---------------------------------------------------------------------- */
/* cron_set_frame callback — drives the scheduler each host frame. */

static void frame (void) {
    /* Spawn-on-next-frame: StartThread queued these into pendingBirth; this
     * is where they actually become coros. Must run on the main thread,
     * which in our model is whoever is executing frame() (the cart's
     * implicit context, which becomes main_coro on the first swap). */
    ProcessThreadLifecycles ();

    /* SLICE-4d: the REAL menu now drives the screen — Starcon2Main runs its
     * while(StartGame()) loop, StartGame -> RestartMenu -> DoInput, which reads
     * PulsedInputState. Feed the pad into ImmediateInputState BEFORE the
     * scheduler so DoInput's own UpdateInputState sees this frame's input. */
    cron_input_poll ();
    Scheduler_CRON_RunFrame (10);  /* ~10 ms budget per frame */
    ++s_frame_no;

    TFB_FlushGraphics ();          /* drain DCQ -> render onto MAIN canvas */
    cron_vid_present ();           /* downsample the 32bpp MAIN screen -> FB */
}

/* ---------------------------------------------------------------------- */
/* File-layer validation: mount the ROM-baked .uqm content pack through the
 * real libs/uio (stdio fs -> zip fs -> miniz inflate) and read a known
 * deflated entry. Proves the whole faithful file stack works on real data
 * BEFORE we wire LoadKernel/graphics. Mirrors options.c mountContentDir,
 * but mounts the zip by known name (the regex-based scan is disabled). */
/* Replaces UQM's options.c prepareContentDir: mount the ROM-baked .uqm and
 * set the global contentDir, so the real LoadKernel (setup.c) can run its
 * loadIndices/LoadColorMap against it. The earlier per-layer validations
 * (uio read, resource index parse, BINTAB load, CreateContext/Display) are
 * retired now that LoadKernel exercises the whole stack for real. Screen
 * dims are set by hand (320x240, UQM native) since the SDL backend's init
 * (pure.c, which normally sets them) isn't compiled. */
static void mount_content (void) {
    static uio_AutoMount *autoMount[] = { NULL };
    extern uio_DirHandle *contentDir;       /* the seam's options.h global */
    extern int ScreenWidth, ScreenHeight;
    ScreenWidth = 320; ScreenHeight = 240;

    cron_rom_mount ("content/uqm-0.8.0-content.uqm");
    uio_init ();
    uio_Repository *repo = uio_openRepository (0);
    if (!repo) { cron_log ("FS: openRepository failed\n", 26); return; }
    uio_MountHandle *cm = uio_mountDir (repo, "/", uio_FSTYPE_STDIO, NULL, NULL,
            "content", autoMount, uio_MOUNT_TOP | uio_MOUNT_RDONLY, NULL);
    if (!cm) { cron_log ("FS: mount STDIO failed\n", 23); return; }
    uio_DirHandle *cd = uio_openDir (repo, "/", 0);
    if (!cd) { cron_log ("FS: openDir / failed\n", 21); return; }
    /* Mount the .uqm ZIP by known name (options.c's regex-based scan is
     * disabled / not compiled). */
    uio_MountHandle *zm = uio_mountDir (repo, "/", uio_FSTYPE_ZIP, cd,
            "uqm-0.8.0-content.uqm", "/", autoMount,
            uio_MOUNT_BELOW | uio_MOUNT_RDONLY, cm);
    if (!zm) { cron_log ("FS: mount ZIP failed\n", 21); return; }
    contentDir = uio_openDir (repo, "/", 0);
    cron_log (contentDir ? "FS: content mounted\n" : "FS: content mount FAILED\n",
              contentDir ? 20 : 25);

    /* InitColorMaps creates the colormap mutex + tables. Normally called by
     * the SDL backend's TFB_InitGraphics (not compiled), so the seam does it
     * — else SetColorMap (in real LoadKernel) locks an uninitialised mutex. */
    extern void InitColorMaps (void);
    InitColorMaps ();

    /* SLICE-3a derisk: prove the uio -> stb PNG decode path works on real
     * content before wiring the cel/font resource loaders. */
    extern void cron_img_selftest (uio_DirHandle *dir);
    if (contentDir) cron_img_selftest (contentDir);
}

int main (void) {
    const char *msg = "cronopio-uqm spike: starting\n";
    cron_log (msg, 29);
    cron_vid_init ();
    mount_content ();
    InitThreadSystem ();

    /* Use StartThread, NOT CreateThread — the latter blocks the caller on a
     * semaphore until ProcessThreadLifecycles spawns the thread, which means
     * the main thread deadlocks if it ever calls it. The threadlib.h
     * comment is explicit about this (line 82). StartThread sets sem=NULL
     * and returns immediately. */
    /* The worker_a/worker_b heartbeat threads were bring-up scaffolding (proved
     * the scheduler dispatches cooperatively); removed now that Starcon2Main
     * reaches its real main loop + menu. */
    /* Starcon2Main needs a big stack: LoadKernel + now the menu (restart.c ->
     * DrawRestartMenuGraphic -> Flash overlay -> font_DrawText) are deep call
     * chains run inline on this coro. 512KB overflowed (corrupted the heap ->
     * garbage coro pointers -> bad CORO_SWAP) right after the menu rendered;
     * 1MB (= CRON_THREAD_STACK_MAX) gives headroom. */
    StartThread (Starcon2Main, NULL, 1024 * 1024, "Starcon2Main");

    cron_input_init ();    /* set up the real input pipeline (gameinp.c) */
    cron_set_frame (frame);
    return 0;
}
