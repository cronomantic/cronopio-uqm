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

/* Cron backend's scheduler entry — declared in
 * sc2/src/libs/threads/cron/cronthreads.h, mirrored here to avoid pulling
 * the backend header (which we want to remain a UQM-only file). */
extern uint32_t Scheduler_CRON_RunFrame (uint32_t tick_budget_ms);

/* Drains pendingBirth/pendingDeath in thrcommon.c — must be called from the
 * main "thread" each frame for StartThread() requests to actually spawn. */
extern void ProcessThreadLifecycles (void);

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
}

/* ---------------------------------------------------------------------- */
/* File-layer validation: mount the ROM-baked .uqm content pack through the
 * real libs/uio (stdio fs -> zip fs -> miniz inflate) and read a known
 * deflated entry. Proves the whole faithful file stack works on real data
 * BEFORE we wire LoadKernel/graphics. Mirrors options.c mountContentDir,
 * but mounts the zip by known name (the regex-based scan is disabled). */
static void uqm_content_test (void) {
    static uio_AutoMount *autoMount[] = { NULL };
    cron_rom_mount ("content/uqm-0.8.0-content.uqm");
    uio_init ();
    uio_Repository *repo = uio_openRepository (0);
    if (!repo) { cron_log ("FS: openRepository failed\n", 26); return; }

    uio_MountHandle *cm = uio_mountDir (repo, "/", uio_FSTYPE_STDIO, NULL, NULL,
            "content", autoMount, uio_MOUNT_TOP | uio_MOUNT_RDONLY, NULL);
    if (!cm) { cron_log ("FS: mount STDIO failed\n", 23); return; }

    uio_DirHandle *cd = uio_openDir (repo, "/", 0);
    if (!cd) { cron_log ("FS: openDir / failed\n", 21); return; }

    /* Mount the baked .uqm ZIP by known name (the regex-based scan in
     * options.c mountDirZips is disabled / not compiled). NOTE: parsing the
     * 10559-entry central directory under the interpreted VM is SLOW
     * (~minutes headless) — a known perf item, fine for this validation. */
    uio_MountHandle *zm = uio_mountDir (repo, "/", uio_FSTYPE_ZIP, cd,
            "uqm-0.8.0-content.uqm", "/", autoMount,
            uio_MOUNT_BELOW | uio_MOUNT_RDONLY, cm);
    if (!zm) { cron_log ("FS: mount ZIP failed\n", 21); return; }

    uio_DirHandle *content = uio_openDir (repo, "/", 0);
    uio_Handle *h = uio_open (content ? content : cd, "uqm.rmp", O_RDONLY, 0);
    if (!h) { cron_log ("FS: open uqm.rmp failed\n", 24); return; }

    char buf[17]; memset (buf, 0, sizeof buf);
    ssize_t n = uio_read (h, buf, 16);
    char m[96];
    int ml = snprintf (m, sizeof m,
            "FS: read uqm.rmp -> %d bytes: \"%s\" (expect colortable.hanga)\n",
            (int)n, buf);
    cron_log (m, ml);
    uio_close (h);

    /* Resource-layer validation: parse the .rmp resource index (963 entries)
     * via libs/resource over uio, then look a resource up by name. This is
     * the next layer above raw uio — InitResourceSystem registers the type
     * vtables (graphic/audio/video handlers are stubbed; not invoked here),
     * LoadResourceIndex parses uqm.rmp into a name->{type,path} map. */
    extern uio_DirHandle *contentDir;       /* the seam's options.h global */
    contentDir = content;
    InitResourceSystem ();
    LoadResourceIndex (contentDir, "uqm.rmp", (const char *)0);
    /* A real registered resource resolves (non-NULL); a bogus name does not.
     * type is UNKNOWNRES (not BINTAB) until libs/strings InstallStringTable-
     * ResType lands — registering BINTAB/STRTAB is the next layer; here we
     * only prove the 963-entry index parsed and resources are queryable. */
    const char *t   = res_GetResourceType ("colortable.hangar");
    const char *bog = res_GetResourceType ("no.such.resource");
    char r[112];
    int rl = snprintf (r, sizeof r,
            "RES: index parsed; colortable.hangar=\"%s\" bogus=%s\n",
            t ? t : "(null)", bog ? bog : "(null,ok)");
    cron_log (r, rl);
}

int main (void) {
    const char *msg = "cronopio-uqm spike: starting\n";
    cron_log (msg, 29);
    uqm_content_test ();
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
    StartThread (Starcon2Main, NULL, 64 * 1024, "Starcon2Main");

    cron_set_frame (frame);
    return 0;
}
