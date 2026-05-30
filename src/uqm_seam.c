/*
 * uqm_seam.c — minimal cart-side stubs for the UQM platform layer.
 *
 * Provides exactly what libs/threads/cron + libs/task need to compile and
 * run:
 *   - HMalloc/HCalloc/HFree (libs/memlib.h) — wrap libc malloc/calloc/free
 *   - log_add/log_addV (compat/log_cron.h) — route through cron_log
 *   - GetTimeCounter (libs/timelib.h) — based on cron_time_ms scaled to
 *     UQM's ONE_SECOND = 840 ticks/sec
 *   - Async_process / Async_timeBeforeNextMs — no-ops for now (no async
 *     callbacks in the spike)
 *
 * Everything else from UQM is either not yet compiled (graphics, sound,
 * input, save, content) or stubbed inside main_cron.c (Starcon2Main).
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cronopio.h>

#include "port.h"
#include "libs/threadlib.h"
#include "libs/memlib.h"
#include "libs/timelib.h"
#include "libs/log.h"
#include "libs/callback/async.h"

/* ---------------------------------------------------------------------- */
/* memlib */

void *HMalloc  (size_t size)            { return malloc (size); }
void *HCalloc  (size_t size)            { void *p = malloc (size); if (p) memset (p, 0, size); return p; }
void *HRealloc (void *p, size_t size)   { return realloc (p, size); }
void  HFree    (void *p)                { free (p); }

/* ---------------------------------------------------------------------- */
/* log — route through cron_log. Drop log_Debug to avoid spamming. */

void log_addV (log_Level lvl, const char *fmt, va_list ap) {
    if (lvl >= log_Debug) return;
    char buf[512];
    int n = vsnprintf (buf, sizeof buf, fmt, ap);
    if (n < 0) return;
    if (n > 0 && buf[n - 1] != '\n' && (size_t)n + 1 < sizeof buf) {
        buf[n] = '\n'; ++n;
    }
    cron_log (buf, n);
}

void log_add (log_Level lvl, const char *fmt, ...) {
    va_list ap; va_start (ap, fmt);
    log_addV (lvl, fmt, ap);
    va_end (ap);
}

void log_showBox (int wait, int doFatal) {
    /* Real uqmlog pops a platform message box; we just route through the
     * normal log channel (msgs were already log_add'd before this call). */
    (void)wait; (void)doFatal;
    cron_log ("[log_showBox]\n", 14);
}

/* ---------------------------------------------------------------------- */
/* Stub globals — externs referenced by starcon.c (and friends) that
 * live in TUs we haven't compiled yet. Each is defined here just enough
 * to satisfy the translator's "extern must have an initializer" check;
 * once the real owning TU lands in the KEEP list these go away. */

#include "libs/compiler.h"   /* BOOLEAN, BYTE, etc. */
#include "libs/gfxlib.h"     /* FRAME, STRING, CONTEXT */
#include "libs/heap.h"       /* QUEUE */
#include "uqm/globdata.h"    /* GLOBDATA, ACTIVITY */
#include "uqm/starmap.h"     /* STAR_DESC */
#include "uqm/element.h"     /* PRIMITIVE, MAX_DISPLAY_PRIMS */

/* GlobData + RadarContext now defined by uqm/globdata.c (in KEEP).
 * GamePaused / CurrentInputState / PulsedInputState / ImmediateInputState now
 * defined by uqm/gameinp.c (in KEEP). */

/* options.h globals — these would be populated by parseOptions() in a
 * real main(); the cart hand-fixes them once content paths are known. */
char baseContentPath[PATH_MAX];
struct uio_DirHandle *contentDir;   /* uio_* are uqm filelib types */
struct uio_DirHandle *configDir;
struct uio_DirHandle *saveDir;
struct uio_DirHandle *meleeDir;

/* ---------------------------------------------------------------------- */
/* timelib — UQM measures time in ONE_SECOND=840 ticks/sec units.
 * GetTimeCounter is the "now" reading; SleepThread/SleepThreadUntil use it
 * via threadlib. Scale from cron_time_ms() (host millisecond counter). */

TimeCount GetTimeCounter (void) {
    uint32_t ms = cron_time_ms ();
    /* 840 ticks / 1000 ms = 21 / 25; multiply then divide to keep precision
     * within u32. ms < ~2^31 stays safe for ~28 days of uptime. */
    return (TimeCount)((ms * 21u) / 25u);
}

/* ---------------------------------------------------------------------- */
/* async — no-op for the spike. UQM uses Async to schedule callbacks fired
 * by the main loop (lander animation, etc.); we'll wire it when we get to
 * the renderer / game loop. */

void   Async_process            (void) { }
uint32 Async_timeBeforeNextMs   (void) { return 1000u; }

/* ---------------------------------------------------------------------- */
/* loadIndices — UQM's real one (options.c) scans the content dir for *.rmp
 * with a POSIX regex (match_MATCH_REGEX), which we disabled and don't
 * compile options.c. The base content has a single index, uqm.rmp, so load
 * it directly by name. Returns the count loaded. */
#include "libs/reslib.h"
#include "libs/uio.h"
int loadIndices (uio_DirHandle *dir) {
    LoadResourceIndex (dir, "uqm.rmp", (const char *)0);
    return 1;
}

/* ---------------------------------------------------------------------- */
/* STUB_EXTERNS_START — auto-discovered externs from tools/stub_externs.sh.
 * Each entry comes from a translator "extern not supported" error;
 * the stub gives the symbol storage so the build advances. These come
 * out as the real owning TUs land in build_uqm.sh's KEEP list. */
int GfxFlags;
/* GraphicsDriver / ScreenWidth(Actual) / ScreenHeight(Actual) — now real,
 * in libs/graphics/gfx_common.c (in KEEP). */
int snddriver, soundflags;
ACTIVITY NextActivity;
/* LastActivity / FlagStatFrame / MiscDataFrame / FontGradFrame / GameStrings /
 * Screen — now defined by uqm/setup.c (in KEEP).
 * master_q — defined by uqm/master.c (in KEEP). */
/* DisplayArray — now defined by uqm/process.c (the ELEMENT display list, in KEEP). */
/* Interplanetary-view globals normally owned by settings.c / hyper.c / lander.c,
 * not yet in KEEP. The opt* flags default to 0 (PC menus/fonts, no melee scale,
 * mono SFX) which is the desired baseline; the rest are state the boundary-
 * stubbed features would own. */
int optWhichMenu;
int optWhichFonts;
int optMeleeScale;
int optWhichCoarseScan;   /* planet-surface scan style (settings.c) */
int optWhichShield;       /* planet shield-throb style (settings.c) */
BOOLEAN optStereoSFX;
FRAME stars_in_space;
POINT SpaceOrg;
volatile int QuitPosted;
BOOLEAN opt3doMusic;
BOOLEAN optSpeech;
BOOLEAN optRemixMusic;
int optWhichIntro;
const char **optAddons;
/* PlayerInput[NUM_PLAYERS] — now defined by uqm/battlecontrols.c (in KEEP),
 * which also owns Human/ComputerInputContext_new. */
SOUND MenuSounds;
SOUND GameSounds;          /* battle sound table (sounds.c) — audio not built */
BYTE battle_counter[2];    /* NUM_SIDES; battle frame counter — battle only */
FRAME explosion[16];       /* ship-explosion animation frames — battle only (zeroed) */
volatile int GameActive;

/* Game-data tables referenced by StartGame's tail (restart.c:399 —
 * star_array=starmap_array; Elements=element_array; PlanData=planet_array).
 * The real tables (starmap_array = the galaxy's stars, element_array,
 * planet_array) now come from uqm/plandata.c (slice 5b: the interplanetary
 * view's FindStar searches starmap_array to locate the current system — the
 * [1] stubs made it "do not know where you are"). The pointers star_array/
 * Elements/PlanData are assigned by restart.c and owned here. */
#include "uqm/planets/plandata.h"   /* PlanetFrame */
/* Elements / PlanData (the pointers restart.c assigns) are now defined by
 * uqm/planets/surface.c (in KEEP) — the planet-surface engine owns them.
 * CreatureData (the life-form table) is now REAL too — uqm/planets/lander.c. */
volatile int MouseButtonDown;
/* pSolarSysState + ExploreSolarSys are now REAL — planets/solarsys.c (slice 5b:
 * the interplanetary solar-system view). The slice-5a behavioural stub that set
 * CHECK_ABORT to bail out of the game loop is gone; New Game now enters the real
 * solar system. */

/* getGenerateFunctions is now REAL — uqm/gendef.c maps a star Index to that
 * system's generator table (Sol -> generateSolFunctions in planets/generate/
 * gensol.c). The behavioural empty-genFuncs stub is gone; the solar system is
 * now populated with its real planets. */
