/*
 * config_cron.h — UQM build configuration for the Cronopio (CronoVM) port.
 * Reached via sc2/src/config.h when -DCRONOPIO is set.
 *
 * Picks the cooperative-coroutine threading backend, disables features that
 * either depend on host facilities we don't have (network, OpenAL, mikmod)
 * or that the cart model can't sensibly carry (NAMED_SYNCHRO /
 * TRACK_CONTENTION — cooperative scheduling means "contention" doesn't
 * have its usual meaning, and dropping these halves the work of porting
 * libs/threads/ wrappers around the backend).
 */

#ifndef CONFIG_CRON_H_
#define CONFIG_CRON_H_

/* Game data dirs — the cart loads everything from the ROM, but UQM's path
 * code references these constants. Pick safe defaults. */
#define CONTENTDIR  "/rom/content/"
#define USERDIR     "/save/"
#define CONFIGDIR   USERDIR
#define MELEEDIR    "/save/teams/"
#define SAVEDIR     "/save/"

/* CronoVM is little-endian (i386-elf target). */
/* #undef WORDS_BIGENDIAN */

/* Cronopio libc provides strcasecmp (libs/strings.h). The rest are absent. */
#define HAVE_STRCASECMP_UQM 1

/* clang on i386-elf provides _Bool, wchar_t, and wint_t. */
#define HAVE__BOOL    1
#define HAVE_WCHAR_T  1
#define HAVE_WINT_T   1

/* port.h's other-platform branches define PATH_MAX from <limits.h>; the
 * Cronopio libc doesn't ship it (no real filesystem) so we name it here.
 * 256 covers anything the cart-side path code might construct (UQM paths
 * are short — content/save/config names + slash separators). */
#ifndef PATH_MAX
#  define PATH_MAX 256
#endif

/* We DON'T actually have readdir_r, but defining it here bypasses port.h's
 * <dirent.h> include + readdir_r prototype that the cart doesn't need.
 * Any code path that calls readdir_r will fail at link/translate time —
 * which is the right signal. */
#define HAVE_READDIR_R 1

/* Same trick: port.h declares strupr() if HAVE_STRUPR is unset. The cart
 * doesn't use it; we set the flag to skip the declaration. */
#define HAVE_STRUPR 1

/* port.h declares setenv() if HAVE_SETENV is unset. We don't use it. */
#define HAVE_SETENV 1

/* We don't have these — leaving them off costs nothing if the code paths
 * aren't reached: HAVE_STRICMP, HAVE_GETOPT_LONG, HAVE_ISWGRAPH. */

/* Pick the cooperative-coroutine threading backend. */
#define THREADLIB_CRON 1

/* NAMED_SYNCHRO and TRACK_CONTENTION are unconditionally #define'd by
 * libs/threadlib.h, so we can't disable them here. Our libs/threads/cron/
 * backend supports both via #ifdef NAMED_SYNCHRO compile paths — it just
 * doesn't do anything useful with the names/classes in the cooperative
 * model. Small cost; not worth a deeper UQM patch. */

#endif /* CONFIG_CRON_H_ */
