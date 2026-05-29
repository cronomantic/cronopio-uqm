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
    UpdateInputState ();
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

/* ---------------------------------------------------------------------- */
/* Stub workers. Real Starcon2Main lives in sc2/src/uqm/starcon.c and is
 * thousands of lines + many subsystems we haven't compiled yet. For the
 * spike, this stub just proves the scheduler dispatches us. */

static int worker_a (void *arg) {
    (void)arg;
    for (int i = 0; i < 5; ++i) {
        log_add (log_Info, "worker_a tick %d", i);
        SleepThread (ONE_SECOND / 4);    /* 250 ms cooperative sleep */
    }
    log_add (log_Info, "worker_a done");
    return 0;
}

static int worker_b (void *arg) {
    (void)arg;
    for (int i = 0; i < 8; ++i) {
        log_add (log_Info, "worker_b tick %d", i);
        TaskSwitch ();                   /* voluntary yield */
        SleepThread (ONE_SECOND / 7);    /* ~143 ms */
    }
    log_add (log_Info, "worker_b done");
    return 0;
}

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
    Scheduler_CRON_RunFrame (10);  /* ~10 ms budget per frame */

    /* SLICE-2a: prove the backend renders. Once UQM's own drawing drives the
     * screen (StartGame TRUE), drop this. */
    if (!s_did_selftest) { cron_gfx_selftest (); s_did_selftest = 1; }

    /* SLICE-3 derisk: once InitKernel has installed the GFXRES/FONTRES handlers
     * (a few frames in), load a real font + cel by name to prove they decode. */
    if (++s_frame_no == 20 && !s_did_res_selftest) {
        void *fnt = res_GetResource ("comm.commander.font");
        void *gfx = res_GetResource ("comm.arilou.graphics");
        log_add (log_User, "RES: font(player.fon)=%s  cel(arilou.ani)=%s",
                 fnt ? "OK" : "NULL", gfx ? "OK" : "NULL");
        /* SLICE-4: load the real new-game menu (navigable demo, see below). */
        extern void cron_menu_init (void);
        cron_menu_init ();
        s_did_res_selftest = 1;
    }

    /* SLICE-4b/c: poll the real input pipeline, then run the navigable menu. */
    {
        extern void cron_menu_frame (int frame_no);
        cron_input_poll ();
        cron_menu_frame (s_frame_no);
    }

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

/* SLICE-4b: a navigable demo of the real new-game menu, drawn through UQM's
 * own drawing API. graphics.newgame (base/ui/newgame.ani) has 6 frames:
 *   frame 0      = full-screen background ("New Game / Load Game / ..."),
 *   frames 1..5  = the highlight overlay for each option (positioned by each
 *                  frame's hotspot).
 * We draw frame 0 + the highlight for the current selection, move the
 * selection with the 12-button pad (UP/DOWN, edge-detected by the host), and
 * auto-cycle it so the highlight is observable headless. This proves multi-
 * frame cel rendering + pad input reaching the cart + selection-driven redraw.
 *
 * This is a DEMO menu (no game action on select yet) -- the faithful menu state
 * machine lives in uqm/restart.c (RestartMenu/DoRestart + the Flash overlay +
 * DoInput + gameinp.c PulsedInputState), whose Melee/Credits/Intro/Setup/save
 * dep tree is the next slice. */
#define CRON_MENU_OPTS 5

static FRAME s_menu_f0;       /* frame 0 = background; 1..5 = highlights */
static int   s_menu_sel;
static int   s_menu_ready;

void cron_menu_init (void) {
    extern CONTEXT ScreenContext;
    DRAWABLE d;
    if (!ScreenContext) { log_add (log_User, "MENU: no ScreenContext"); return; }
    d = (DRAWABLE)LoadGraphicInstance ("graphics.newgame");
    s_menu_f0 = CaptureDrawable (d);
    if (!s_menu_f0) { log_add (log_User, "MENU: load graphics.newgame FAILED"); return; }
    s_menu_ready = 1;
    log_add (log_User, "MENU: loaded graphics.newgame (navigable demo)");
}

static void cron_menu_draw (void) {
    extern CONTEXT ScreenContext;
    extern int ScreenWidth, ScreenHeight;
    RECT r;
    STAMP s;

    SetContext (ScreenContext);
    SetContextBackGroundColor (BUILD_COLOR_RGBA (0, 0, 0, 255));
    ClearDrawable ();

    /* background (centered; it is full 320x240, so this is (0,0)) */
    GetFrameRect (s_menu_f0, &r);
    s.origin.x = (ScreenWidth  - r.extent.width)  >> 1;
    s.origin.y = (ScreenHeight - r.extent.height) >> 1;
    s.frame = s_menu_f0;
    DrawStamp (&s);

    /* highlight overlay for the current selection (frame sel+1; its own
     * hotspot positions it over the right menu line) */
    s.origin.x = 0;
    s.origin.y = 0;
    s.frame = SetAbsFrameIndex (s_menu_f0, (COUNT)(s_menu_sel + 1));
    DrawStamp (&s);
}

void cron_menu_frame (int frame_no) {
    if (!s_menu_ready)
        return;
    /* Read the REAL PulsedInputState (driven by cron_input_poll -> the pad ->
     * UpdateInputState pulse logic). */
    if (PulsedInputState.menu[KEY_MENU_UP])
        s_menu_sel = (s_menu_sel + CRON_MENU_OPTS - 1) % CRON_MENU_OPTS;
    if (PulsedInputState.menu[KEY_MENU_DOWN])
        s_menu_sel = (s_menu_sel + 1) % CRON_MENU_OPTS;
    if (PulsedInputState.menu[KEY_MENU_SELECT])
        log_add (log_User, "MENU: selected option %d", s_menu_sel);
    /* auto-cycle once a second so the highlight is observable headless (no pad
     * injection there); harmless on-device alongside real input. */
    if (frame_no > 0 && (frame_no % 60) == 0)
        s_menu_sel = (s_menu_sel + 1) % CRON_MENU_OPTS;
    cron_menu_draw ();
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
    /* Workers are bring-up scaffolding — they prove the scheduler dispatches
     * us. Keep them around for now; remove once Starcon2Main reaches its
     * main loop and we don't need the heartbeat-progress signal. */
    StartThread (worker_a,     NULL, 64 * 1024, "worker_a");
    StartThread (worker_b,     NULL, 64 * 1024, "worker_b");
    /* Starcon2Main needs a big stack: LoadKernel loads + inflates + parses
     * resources (the colormap, later fonts/graphics) inline, which overflowed
     * a 64KB coro stack (LoadColorMap silently returned NULL → fatal). */
    StartThread (Starcon2Main, NULL, 512 * 1024, "Starcon2Main");

    cron_input_init ();    /* set up the real input pipeline (gameinp.c) */
    cron_set_frame (frame);
    return 0;
}
