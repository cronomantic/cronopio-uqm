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

#include <cronopio.h>

#include "port.h"
#include "libs/threadlib.h"
#include "libs/log.h"
#include "libs/timelib.h"

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

static int stub_starcon2main (void *arg) {
    (void)arg;
    for (int i = 0; i < 3; ++i) {
        log_add (log_Info, "Starcon2Main heartbeat %d", i);
        SleepThread (ONE_SECOND / 2);
    }
    log_add (log_Info, "Starcon2Main exiting (stub)");
    return 0;
}

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

int main (void) {
    const char *msg = "cronopio-uqm spike: starting\n";
    cron_log (msg, 29);
    InitThreadSystem ();

    /* Use StartThread, NOT CreateThread — the latter blocks the caller on a
     * semaphore until ProcessThreadLifecycles spawns the thread, which means
     * the main thread deadlocks if it ever calls it. The threadlib.h
     * comment is explicit about this (line 82). StartThread sets sem=NULL
     * and returns immediately. */
    StartThread (worker_a,          NULL, 64 * 1024, "worker_a");
    StartThread (worker_b,          NULL, 64 * 1024, "worker_b");
    StartThread (stub_starcon2main, NULL, 64 * 1024, "Starcon2Main");

    cron_set_frame (frame);
    return 0;
}
