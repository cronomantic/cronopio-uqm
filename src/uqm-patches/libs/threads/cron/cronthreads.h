/*
 *  cronthreads.h — CronoVM cooperative-coroutine backend for libs/threads/
 *
 *  Drop-in replacement for sdlthreads.h / posixthreads.h. Mapping:
 *    NativeFoo  ->  Foo_CRON
 *  All sync primitives are cart-side (no host syscalls beyond cron_coro_*).
 *  See `cronthreads.c` for the scheduler.
 *
 *  This file lives in the UQM tree at sc2/src/libs/threads/cron/cronthreads.h
 *  on the cronopio-port branch of the fork. Staged in cronopio-uqm/src/
 *  uqm-patches/libs/threads/cron/ until that branch is cut.
 *
 *  Released under the same GPLv2-or-later as the rest of UQM.
 */

#ifndef LIBS_THREADS_CRON_CRONTHREADS_H_
#define LIBS_THREADS_CRON_CRONTHREADS_H_

#include "port.h"
#include "libs/threadlib.h"
#include "libs/timelib.h"

void InitThreadSystem_CRON   (void);
void UnInitThreadSystem_CRON (void);

#ifdef NAMED_SYNCHRO
/* Prototypes with the "name" field — NAMED_SYNCHRO build. The cooperative
 * model means TRACK_CONTENTION is degenerate (no contention exists if we
 * never preempt) but NAMED_SYNCHRO is still useful for debug logs. */
Thread         CreateThread_CRON          (ThreadFunction func, void *data, SDWORD stackSize, const char *name);
Mutex          CreateMutex_CRON           (const char *name, DWORD syncClass);
Semaphore      CreateSemaphore_CRON       (DWORD initial, const char *name, DWORD syncClass);
RecursiveMutex CreateRecursiveMutex_CRON  (const char *name, DWORD syncClass);
CondVar        CreateCondVar_CRON         (const char *name, DWORD syncClass);
#else
Thread         CreateThread_CRON          (ThreadFunction func, void *data, SDWORD stackSize);
Mutex          CreateMutex_CRON           (void);
Semaphore      CreateSemaphore_CRON       (DWORD initial);
RecursiveMutex CreateRecursiveMutex_CRON  (void);
CondVar        CreateCondVar_CRON         (void);
#endif

ThreadLocal *GetMyThreadLocal_CRON  (void);

void SleepThread_CRON       (TimeCount sleepTime);
void SleepThreadUntil_CRON  (TimeCount wakeTime);
void TaskSwitch_CRON        (void);
void WaitThread_CRON        (Thread thread, int *status);
void DestroyThread_CRON     (Thread thread);

void DestroyMutex_CRON      (Mutex m);
void LockMutex_CRON         (Mutex m);
void UnlockMutex_CRON       (Mutex m);

void DestroySemaphore_CRON  (Semaphore sem);
void SetSemaphore_CRON      (Semaphore sem);
void ClearSemaphore_CRON    (Semaphore sem);

void DestroyCondVar_CRON    (CondVar c);
void WaitCondVar_CRON       (CondVar c);
void SignalCondVar_CRON     (CondVar c);
void BroadcastCondVar_CRON  (CondVar c);

void DestroyRecursiveMutex_CRON     (RecursiveMutex m);
void LockRecursiveMutex_CRON        (RecursiveMutex m);
void UnlockRecursiveMutex_CRON      (RecursiveMutex m);
int  GetRecursiveMutexDepth_CRON    (RecursiveMutex m);

/* Cooperative-scheduler entry point — called by the cart's frame() once
 * per host frame. Runs runnable coroutines until: (a) no thread is runnable
 * (everyone is sleeping/blocked) — returns early so host gets time; or
 * (b) `tick_budget_ms` of cart wall time has elapsed — returns to keep the
 * frame budget. Returns the number of µs the scheduler actually consumed. */
uint32_t Scheduler_CRON_RunFrame (uint32_t tick_budget_ms);

#define NativeInitThreadSystem      InitThreadSystem_CRON
#define NativeUnInitThreadSystem    UnInitThreadSystem_CRON

#define NativeGetMyThreadLocal      GetMyThreadLocal_CRON

#define NativeCreateThread          CreateThread_CRON
#define NativeSleepThread           SleepThread_CRON
#define NativeSleepThreadUntil      SleepThreadUntil_CRON
#define NativeTaskSwitch            TaskSwitch_CRON
#define NativeWaitThread            WaitThread_CRON
#define NativeDestroyThread         DestroyThread_CRON

#define NativeCreateMutex           CreateMutex_CRON
#define NativeDestroyMutex          DestroyMutex_CRON
#define NativeLockMutex             LockMutex_CRON
#define NativeUnlockMutex           UnlockMutex_CRON

#define NativeCreateSemaphore       CreateSemaphore_CRON
#define NativeDestroySemaphore      DestroySemaphore_CRON
#define NativeSetSemaphore          SetSemaphore_CRON
#define NativeClearSemaphore        ClearSemaphore_CRON

#define NativeCreateCondVar         CreateCondVar_CRON
#define NativeDestroyCondVar        DestroyCondVar_CRON
#define NativeWaitCondVar           WaitCondVar_CRON
#define NativeSignalCondVar         SignalCondVar_CRON
#define NativeBroadcastCondVar      BroadcastCondVar_CRON

#define NativeCreateRecursiveMutex      CreateRecursiveMutex_CRON
#define NativeDestroyRecursiveMutex     DestroyRecursiveMutex_CRON
#define NativeLockRecursiveMutex        LockRecursiveMutex_CRON
#define NativeUnlockRecursiveMutex      UnlockRecursiveMutex_CRON
#define NativeGetRecursiveMutexDepth    GetRecursiveMutexDepth_CRON

#endif /* LIBS_THREADS_CRON_CRONTHREADS_H_ */
