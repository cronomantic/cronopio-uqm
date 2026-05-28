/*
 * log_cron.h — minimal stub for UQM's libs/log.h, selected when CRONOPIO is
 * defined (via the patched libs/log.h). The real uqmlog.h depends on
 * msgbox UI, <windows.h>, <signal.h>, and threading internals — none of
 * which the cart wants. We provide just enough to compile callers
 * (log_Level enum + log_add prototype).
 *
 * Implementation lives in src/uqm_seam.c and routes through cron_log.
 */

#ifndef LOG_CRON_H_
#define LOG_CRON_H_

#include <stdarg.h>

typedef enum {
    log_Nothing = 0,
    log_User,
    log_Fatal = log_User,
    log_Error,
    log_Warning,
    log_Info,
    log_Debug
} log_Level;

extern void log_add  (log_Level, const char *fmt, ...);
extern void log_addV (log_Level, const char *fmt, va_list);

/* The "_nothread" variants the real uqmlog has are aliased to log_add. */
#define log_add_nothread  log_add
#define log_add_nothreadV log_addV

/* Real uqmlog pops a platform message box. Our stub is a no-op — UQM
 * uses this to surface fatal errors that the user should see; in the
 * cart we route them through cron_log already via log_add. */
extern void log_showBox (int wait, int doFatal);

#endif /* LOG_CRON_H_ */
