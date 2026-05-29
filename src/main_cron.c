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
#include "vid_cron.h"

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
    /* Workers are bring-up scaffolding — they prove the scheduler dispatches
     * us. Keep them around for now; remove once Starcon2Main reaches its
     * main loop and we don't need the heartbeat-progress signal. */
    StartThread (worker_a,     NULL, 64 * 1024, "worker_a");
    StartThread (worker_b,     NULL, 64 * 1024, "worker_b");
    /* Starcon2Main needs a big stack: LoadKernel loads + inflates + parses
     * resources (the colormap, later fonts/graphics) inline, which overflowed
     * a 64KB coro stack (LoadColorMap silently returned NULL → fatal). */
    StartThread (Starcon2Main, NULL, 512 * 1024, "Starcon2Main");

    cron_set_frame (frame);
    return 0;
}
