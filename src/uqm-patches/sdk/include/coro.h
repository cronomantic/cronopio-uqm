/*
 *  coro.h — Proposed Cronopio SDK header for cooperative coroutines.
 *
 *  STAGED — this header is not yet part of Cronopio. The final location is
 *      third_party/Cronopio/sdk/include/coro.h
 *  on the Cronopio main branch, once the cron_coro_* opcodes (0x3C, 0x3D)
 *  ship in CronoVM. See memory `cronovm-coro-design` for the full design.
 *
 *  Released under the same license as the rest of the Cronopio SDK.
 */

#ifndef _CVM_CORO_H
#define _CVM_CORO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Coroutine state.
 *
 * The first three words (_pc, _sp, _dest) are VM-managed — laid out
 * identically to a jmp_buf so the same save/restore opcodes operate on both.
 * Do NOT write to them from cart code.
 *
 * The remaining fields are cart-side configuration; the cart fills
 * fn / arg / stack_lo / stack_sz before calling cron_coro_init, and may
 * read `status` and `resumer` after that. The trampoline lives in cart
 * code (the SDK ships a default one as `__cron_coro_trampoline` but a cart
 * may replace it by setting coro->fn to its own entry — see below).
 */
typedef struct cron_coro {
    /* VM-managed context — DO NOT touch from cart code. */
    uint32_t _pc;
    uint32_t _sp;
    uint32_t _dest;

    /* Public state. Updated by the VM on init/swap and by the trampoline. */
    uint32_t status;

    /* Cart-managed. Set BEFORE calling cron_coro_init. */
    void   (*fn)(void *arg);        /* entry — runs on the new stack */
    void    *arg;                   /* first-arg passed to fn */
    void    *stack_lo;              /* lowest address of the stack region */
    uint32_t stack_sz;              /* size of the stack region in bytes */

    /* Updated by CORO_SWAP — the coro that last swapped INTO us. The
     * default trampoline rides off into it when fn returns. */
    struct cron_coro *resumer;
} cron_coro_t;

enum {
    CORO_FRESH     = 0,   /* never run; cron_coro_init has set up context */
    CORO_RUNNING   = 1,   /* currently executing */
    CORO_SUSPENDED = 2,   /* saved by CORO_SWAP, can be resumed */
    CORO_DEAD      = 3    /* fn returned; cannot be swapped to */
};

/* Initialize `coro` for first execution.
 *
 * REQUIRES: coro->fn != NULL, coro->stack_lo != NULL, coro->stack_sz large
 * enough to hold one call frame + reasonable headroom (recommend at least
 * 8 KB; UQM-style hosts use 64 KB).
 *
 * Synthesises a context such that the first `cron_coro_swap(*, coro)` jumps
 * into `coro->fn(coro->arg)` running on the supplied stack. Sets
 * status = CORO_FRESH. Idempotent if status was already FRESH.
 *
 * After the trampoline returns from coro->fn, status becomes CORO_DEAD and
 * control swaps to coro->resumer (or traps if resumer is NULL).
 *
 * Lowered to opcode CORO_INIT (0x3C). */
void cron_coro_init (cron_coro_t *coro);

/* Cooperative context swap.
 *
 * Saves the running cart context into `from` (which becomes
 * status = CORO_SUSPENDED), restores `to` (which becomes CORO_RUNNING),
 * and sets to->resumer = from.
 *
 * Returns to the caller when somebody else swaps back to `from`.
 *
 * REQUIRES: to->status is CORO_FRESH or CORO_SUSPENDED. Swapping to a DEAD
 * or RUNNING coro traps CVM_E_BAD_CORO_STATE.
 *
 * `from` may be a freshly-zeroed cron_coro_t (e.g. a "main" / scheduler
 * coro that has no fn/stack of its own — the cart's existing stack becomes
 * its implicit context).
 *
 * Lowered to opcode CORO_SWAP (0x3D). */
void cron_coro_swap (cron_coro_t *from, cron_coro_t *to);

/* Convenience: swap back to whoever last resumed us.
 *  static inline */
static inline void cron_coro_yield (cron_coro_t *self) {
    if (self->resumer) cron_coro_swap (self, self->resumer);
}

#ifdef __cplusplus
}
#endif

#endif /* _CVM_CORO_H */
