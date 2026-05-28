/*
 *  cronthreads.c — CronoVM cooperative-coroutine backend (skeleton)
 *
 *  Implements UQM's libs/threads/ API on top of cron_coro_* (see Cronopio
 *  sdk/include/coro.h). Single-threaded cooperative model — every native-side
 *  blocking op (LockMutex, WaitCondVar, SetSemaphore-on-0, SleepThread)
 *  yields to the scheduler instead of waiting. The scheduler picks the next
 *  runnable thread and swaps to it.
 *
 *  CALL GRAPH:
 *    cart frame()
 *      └── Scheduler_CRON_RunFrame(budget_ms)
 *            └── pick runnable thread → cron_coro_swap(&sched_coro, &t->coro)
 *                  ├── (thread runs, eventually blocks/yields:)
 *                  └── LockMutex_CRON / WaitCondVar_CRON / SleepThread_CRON
 *                        └── cron_coro_swap(&t->coro, &sched_coro)
 *
 *  This is a SKELETON. It compiles once CronoVM ships:
 *    - opcode 0x3C CORO_INIT
 *    - opcode 0x3D CORO_SWAP
 *    - SDK header sdk/include/coro.h (declared cron_coro_t / fns)
 *    - SDK runtime sdk/lib/cvm_coro.c (trampoline)
 *
 *  Released under the same GPLv2-or-later as the rest of UQM.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port.h"
#include "libs/threadlib.h"
#include "libs/timelib.h"
#include "libs/log.h"
#include "libs/memlib.h"
#include "cronthreads.h"

#include <coro.h>           /* Cronopio SDK: cron_coro_t / init / swap */
#include <cronopio.h>       /* cron_time_ms() for budgeting */

/* ----------------------------------------------------------------------- */
/* Cart-private thread record. UQM hands these out as opaque `Thread`. */

#define CRON_THREAD_STACK_DEFAULT  (64 * 1024)   /* default stack per thread */
#define CRON_THREAD_STACK_MAX      (256 * 1024)
#define CRON_MAX_THREADS           32

typedef enum {
    THR_NEW = 0,        /* fresh, never run */
    THR_RUNNABLE,
    THR_RUNNING,
    THR_BLOCKED_MUTEX,
    THR_BLOCKED_SEM,
    THR_BLOCKED_COND,
    THR_BLOCKED_JOIN,
    THR_SLEEPING,       /* waiting on wake_at */
    THR_DEAD
} ThreadState;

typedef struct CronThread {
    cron_coro_t  coro;          /* VM-managed context — must be first */
    ThreadFunction func;
    void        *data;
    void        *stack;         /* free()'d on join+destroy */
    SDWORD       stack_sz;

    ThreadState  state;
    int          retval;        /* set when func returns */
    TimeCount    wake_at;       /* when SLEEPING */

    /* Wait queue links — intrusive doubly-linked into mutex/sem/cond/join.
     * Distinct from all_next to avoid clobbering each other (a thread on a
     * wait queue is still listed in s_all_head). */
    struct CronThread *q_next;
    struct CronThread *q_prev;

    /* Singly-linked through the global all-threads list. */
    struct CronThread *all_next;

    /* Joiner (set when another thread is WaitThread'ing us) */
    struct CronThread *joiner;

    ThreadLocal  local;

#ifdef THREAD_NAMES
    const char  *name;
#endif
} CronThread;

/* Sync primitives — also opaque to UQM (Mutex / Semaphore / etc are void*). */

typedef struct CronMutex {
    CronThread  *holder;        /* NULL = unheld */
    CronThread  *wait_head;     /* FIFO of blocked threads */
    CronThread  *wait_tail;
#ifdef NAMED_SYNCHRO
    const char  *name;
    DWORD        sync_class;
#endif
} CronMutex;

typedef struct CronRecMutex {
    CronMutex    base;
    int          depth;
} CronRecMutex;

typedef struct CronSem {
    int          value;
    CronThread  *wait_head;
    CronThread  *wait_tail;
#ifdef NAMED_SYNCHRO
    const char  *name;
    DWORD        sync_class;
#endif
} CronSem;

typedef struct CronCond {
    CronThread  *wait_head;
    CronThread  *wait_tail;
#ifdef NAMED_SYNCHRO
    const char  *name;
    DWORD        sync_class;
#endif
} CronCond;

/* ----------------------------------------------------------------------- */
/* Globals: scheduler coro + ready queue. */

static cron_coro_t s_sched_coro;    /* the scheduler's own context */
static CronThread *s_current;        /* the running CronThread (or NULL if in sched) */
static CronThread *s_ready_head;
static CronThread *s_ready_tail;
static CronThread *s_all_head;       /* every alive thread, for sleeping/global passes */

static void ready_push (CronThread *t);
static CronThread *ready_pop (void);
static void wait_push (CronThread **head, CronThread **tail, CronThread *t);
static void wait_remove (CronThread **head, CronThread **tail, CronThread *t);
static CronThread *wait_pop_one (CronThread **head, CronThread **tail);

/* ----------------------------------------------------------------------- */
/* Init / shutdown */

void
InitThreadSystem_CRON (void) {
    s_current = NULL;
    s_ready_head = s_ready_tail = NULL;
    s_all_head = NULL;
    memset (&s_sched_coro, 0, sizeof s_sched_coro);
    /* s_sched_coro is "live" — first CORO_SWAP from sched-to-thread will
     * capture our current cart state into it automatically. No init needed. */
}

void
UnInitThreadSystem_CRON (void) {
    /* Walk s_all_head, free stacks + records of every alive thread (UQM is
     * shutting down). */
    CronThread *t = s_all_head;
    while (t) {
        CronThread *next = t->all_next;
        if (t->stack) HFree (t->stack);
        HFree (t);
        t = next;
    }
    s_all_head = NULL;
}

/* ----------------------------------------------------------------------- */
/* Trampoline — runs once at the start of each new thread, on its own stack. */

static void
cron_thread_entry (void *arg) {
    CronThread *t = (CronThread *)arg;
    t->state = THR_RUNNING;
    t->retval = t->func (t->data);
    t->state = THR_DEAD;
    if (t->joiner) {
        t->joiner->state = THR_RUNNABLE;
        ready_push (t->joiner);
        t->joiner = NULL;
    }
    /* Return to the scheduler. It will leave us in THR_DEAD; DestroyThread
     * (called via WaitThread) cleans up the stack. */
    cron_coro_swap (&t->coro, &s_sched_coro);
    /* unreachable */
}

/* ----------------------------------------------------------------------- */
/* CreateThread / StartThread / WaitThread / DestroyThread */

#ifdef NAMED_SYNCHRO
Thread
CreateThread_CRON (ThreadFunction func, void *data, SDWORD stackSize, const char *name)
#else
Thread
CreateThread_CRON (ThreadFunction func, void *data, SDWORD stackSize)
#endif
{
    SDWORD ss = stackSize > 0 ? stackSize : CRON_THREAD_STACK_DEFAULT;
    if (ss < CRON_THREAD_STACK_DEFAULT) ss = CRON_THREAD_STACK_DEFAULT;
    if (ss > CRON_THREAD_STACK_MAX)     ss = CRON_THREAD_STACK_MAX;

    CronThread *t = HCalloc (sizeof *t);
    t->func = func;
    t->data = data;
    t->stack = HMalloc (ss);
    t->stack_sz = ss;
    t->state = THR_NEW;

    /* Build the coro */
    t->coro.fn = cron_thread_entry;
    t->coro.arg = t;
    t->coro.stack_lo = t->stack;
    t->coro.stack_sz = ss;
    cron_coro_init (&t->coro);

#ifdef THREAD_NAMES
    t->name = name;
#endif

    /* Link into all-threads list via all_next (q_next/q_prev are reserved
     * for the wait queues). */
    t->all_next = s_all_head;
    s_all_head = t;

    t->state = THR_RUNNABLE;
    ready_push (t);
    return (Thread)t;
}

void
WaitThread_CRON (Thread thread, int *status) {
    CronThread *t = (CronThread *)thread;
    if (t->state != THR_DEAD) {
        t->joiner = s_current;
        s_current->state = THR_BLOCKED_JOIN;
        cron_coro_swap (&s_current->coro, &s_sched_coro);
    }
    if (status) *status = t->retval;
    /* UQM expects WaitThread to also reap. */
    DestroyThread_CRON ((Thread)t);
}

void
DestroyThread_CRON (Thread thread) {
    CronThread *t = (CronThread *)thread;
    /* Remove from all-list — O(n), acceptable for ~10 threads max */
    CronThread **pp = &s_all_head;
    while (*pp && *pp != t) pp = &(*pp)->all_next;
    if (*pp) *pp = t->all_next;
    if (t->stack) HFree (t->stack);
    HFree (t);
}

/* ----------------------------------------------------------------------- */
/* Sleep / TaskSwitch */

void
TaskSwitch_CRON (void) {
    /* Cooperative voluntary yield. Push self to back of ready queue. */
    s_current->state = THR_RUNNABLE;
    ready_push (s_current);
    cron_coro_swap (&s_current->coro, &s_sched_coro);
}

void
SleepThread_CRON (TimeCount sleepTime) {
    s_current->state = THR_SLEEPING;
    s_current->wake_at = GetTimeCounter () + sleepTime;
    cron_coro_swap (&s_current->coro, &s_sched_coro);
}

void
SleepThreadUntil_CRON (TimeCount wakeTime) {
    s_current->state = THR_SLEEPING;
    s_current->wake_at = wakeTime;
    cron_coro_swap (&s_current->coro, &s_sched_coro);
}

/* ----------------------------------------------------------------------- */
/* Mutex */

#ifdef NAMED_SYNCHRO
Mutex CreateMutex_CRON (const char *name, DWORD syncClass)
#else
Mutex CreateMutex_CRON (void)
#endif
{
    CronMutex *m = HCalloc (sizeof *m);
#ifdef NAMED_SYNCHRO
    m->name = name;
    m->sync_class = syncClass;
#endif
    return (Mutex)m;
}

void DestroyMutex_CRON (Mutex mx) { HFree (mx); }

void
LockMutex_CRON (Mutex mx) {
    CronMutex *m = (CronMutex *)mx;
    if (m->holder == NULL) {
        m->holder = s_current;
        return;
    }
    /* Block */
    s_current->state = THR_BLOCKED_MUTEX;
    wait_push (&m->wait_head, &m->wait_tail, s_current);
    cron_coro_swap (&s_current->coro, &s_sched_coro);
    /* Resumed by UnlockMutex transferring ownership to us */
}

void
UnlockMutex_CRON (Mutex mx) {
    CronMutex *m = (CronMutex *)mx;
    CronThread *next = wait_pop_one (&m->wait_head, &m->wait_tail);
    if (next) {
        /* Hand off ownership directly (avoids racy lock-then-unlock spec). */
        m->holder = next;
        next->state = THR_RUNNABLE;
        ready_push (next);
    } else {
        m->holder = NULL;
    }
}

/* ----------------------------------------------------------------------- */
/* Semaphore — counting */

#ifdef NAMED_SYNCHRO
Semaphore CreateSemaphore_CRON (DWORD initial, const char *name, DWORD syncClass)
#else
Semaphore CreateSemaphore_CRON (DWORD initial)
#endif
{
    CronSem *s = HCalloc (sizeof *s);
    s->value = (int)initial;
#ifdef NAMED_SYNCHRO
    s->name = name;
    s->sync_class = syncClass;
#endif
    return (Semaphore)s;
}

void DestroySemaphore_CRON (Semaphore sem) { HFree (sem); }

/* SetSemaphore = wait (decrement, block if 0) in UQM's vocabulary.
 * ClearSemaphore = post (increment, wake one waiter). */
void
SetSemaphore_CRON (Semaphore sem) {
    CronSem *s = (CronSem *)sem;
    if (s->value > 0) {
        s->value--;
        return;
    }
    s_current->state = THR_BLOCKED_SEM;
    wait_push (&s->wait_head, &s->wait_tail, s_current);
    cron_coro_swap (&s_current->coro, &s_sched_coro);
}

void
ClearSemaphore_CRON (Semaphore sem) {
    CronSem *s = (CronSem *)sem;
    CronThread *next = wait_pop_one (&s->wait_head, &s->wait_tail);
    if (next) {
        next->state = THR_RUNNABLE;
        ready_push (next);
    } else {
        s->value++;
    }
}

/* ----------------------------------------------------------------------- */
/* CondVar */

#ifdef NAMED_SYNCHRO
CondVar CreateCondVar_CRON (const char *name, DWORD syncClass)
#else
CondVar CreateCondVar_CRON (void)
#endif
{
    CronCond *c = HCalloc (sizeof *c);
#ifdef NAMED_SYNCHRO
    c->name = name;
    c->sync_class = syncClass;
#endif
    return (CondVar)c;
}

void DestroyCondVar_CRON (CondVar c) { HFree (c); }

void
WaitCondVar_CRON (CondVar cv) {
    CronCond *c = (CronCond *)cv;
    s_current->state = THR_BLOCKED_COND;
    wait_push (&c->wait_head, &c->wait_tail, s_current);
    cron_coro_swap (&s_current->coro, &s_sched_coro);
}

void
SignalCondVar_CRON (CondVar cv) {
    CronCond *c = (CronCond *)cv;
    CronThread *next = wait_pop_one (&c->wait_head, &c->wait_tail);
    if (next) {
        next->state = THR_RUNNABLE;
        ready_push (next);
    }
}

void
BroadcastCondVar_CRON (CondVar cv) {
    CronCond *c = (CronCond *)cv;
    CronThread *t;
    while ((t = wait_pop_one (&c->wait_head, &c->wait_tail))) {
        t->state = THR_RUNNABLE;
        ready_push (t);
    }
}

/* ----------------------------------------------------------------------- */
/* RecursiveMutex */

#ifdef NAMED_SYNCHRO
RecursiveMutex CreateRecursiveMutex_CRON (const char *name, DWORD syncClass)
#else
RecursiveMutex CreateRecursiveMutex_CRON (void)
#endif
{
    CronRecMutex *m = HCalloc (sizeof *m);
#ifdef NAMED_SYNCHRO
    m->base.name = name;
    m->base.sync_class = syncClass;
#endif
    return (RecursiveMutex)m;
}

void DestroyRecursiveMutex_CRON (RecursiveMutex rm) { HFree (rm); }

void
LockRecursiveMutex_CRON (RecursiveMutex rm) {
    CronRecMutex *m = (CronRecMutex *)rm;
    if (m->base.holder == s_current) { m->depth++; return; }
    LockMutex_CRON ((Mutex)&m->base);
    m->depth = 1;
}

void
UnlockRecursiveMutex_CRON (RecursiveMutex rm) {
    CronRecMutex *m = (CronRecMutex *)rm;
    if (--m->depth == 0) UnlockMutex_CRON ((Mutex)&m->base);
}

int
GetRecursiveMutexDepth_CRON (RecursiveMutex rm) {
    return ((CronRecMutex *)rm)->depth;
}

/* ----------------------------------------------------------------------- */
/* Thread-local data */

ThreadLocal *
GetMyThreadLocal_CRON (void) {
    return s_current ? &s_current->local : NULL;
}

/* ----------------------------------------------------------------------- */
/* The scheduler. Called from the cart's frame(). */

uint32_t
Scheduler_CRON_RunFrame (uint32_t tick_budget_ms) {
    uint32_t start = cron_time_ms ();
    uint32_t deadline = start + tick_budget_ms;

    /* Wake any sleepers whose timer has expired. O(n) scan of all-list. */
    TimeCount now = GetTimeCounter ();
    for (CronThread *t = s_all_head; t; t = t->all_next) {
        if (t->state == THR_SLEEPING && now >= t->wake_at) {
            t->state = THR_RUNNABLE;
            ready_push (t);
        }
    }

    while (s_ready_head && cron_time_ms () < deadline) {
        CronThread *t = ready_pop ();
        if (t->state != THR_RUNNABLE) continue;  /* stale entry */
        s_current = t;
        t->state = THR_RUNNING;
        cron_coro_swap (&s_sched_coro, &t->coro);
        s_current = NULL;
        /* Thread yielded — its state was already updated by whatever
         * blocking/yield call returned us here. */
    }
    return cron_time_ms () - start;
}

/* ----------------------------------------------------------------------- */
/* Queue helpers */

static void
ready_push (CronThread *t) {
    t->q_next = NULL;
    t->q_prev = s_ready_tail;
    if (s_ready_tail) s_ready_tail->q_next = t;
    else s_ready_head = t;
    s_ready_tail = t;
}

static CronThread *
ready_pop (void) {
    CronThread *t = s_ready_head;
    if (!t) return NULL;
    s_ready_head = t->q_next;
    if (s_ready_head) s_ready_head->q_prev = NULL;
    else s_ready_tail = NULL;
    t->q_next = t->q_prev = NULL;
    return t;
}

static void
wait_push (CronThread **head, CronThread **tail, CronThread *t) {
    t->q_next = NULL;
    t->q_prev = *tail;
    if (*tail) (*tail)->q_next = t;
    else *head = t;
    *tail = t;
}

static CronThread *
wait_pop_one (CronThread **head, CronThread **tail) {
    CronThread *t = *head;
    if (!t) return NULL;
    *head = t->q_next;
    if (*head) (*head)->q_prev = NULL;
    else *tail = NULL;
    t->q_next = t->q_prev = NULL;
    return t;
}

static void
wait_remove (CronThread **head, CronThread **tail, CronThread *t) {
    if (t->q_prev) t->q_prev->q_next = t->q_next;
    else *head = t->q_next;
    if (t->q_next) t->q_next->q_prev = t->q_prev;
    else *tail = t->q_prev;
    t->q_next = t->q_prev = NULL;
}

/* All open questions from the design memory are now resolved:
 *   - ABI arg0 reg = R0 (verified in translator.c) → cron_coro_init writes
 *     _dest = 0; the trampoline reads `self` from R0.
 *   - trampoline resolution: cart writes _pc = (uintptr_t)&trampoline_fn,
 *     status=FRESH; the opcode does FUNCS[pc] on first swap.
 *   - sentinel-pc at stack top: cron_coro_init plants 0xFFFFFFFE there,
 *     stray RET faults via CVM_E_BAD_PC.
 *   - re-entry into DEAD coro: CORO_SWAP traps CVM_E_BAD_CORO_STATE.
 *   - q_next overload: FIXED — CronThread has dedicated all_next field.
 */
