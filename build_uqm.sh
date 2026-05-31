#!/usr/bin/env bash
# Build the Cronopio UQM cartridge.
#
# Strategy (see memory uqm-compile-core-not-stub): compile the WHOLE UQM game
# core — every .c under sc2/src/uqm/ + sc2/src/libs/ EXCEPT the platform
# backends — and stub ONLY the platform seam (audio/video/input/gfx-backend),
# which src/uqm_stubs_link.c + the SDL_Surface shim + picolibc provide. The TU
# set is discovered with one `find` (CORE_SRCS), not a hand-maintained KEEP list.
# tools/measure_core.sh proves all 269 compile with zero translator gaps and the
# undefined residual is exactly the seam.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UQM="$ROOT/third_party/uqm/sc2"
US="$UQM/src"

# Cronopio SDK submodule (nested CronoVM + TinySoundFont). Init if missing.
CRONOPIO="$ROOT/third_party/Cronopio"
if [[ ! -f "$CRONOPIO/CMakeLists.txt" ]]; then
  echo "[build] Cronopio submodule missing — initialising..."
  git -C "$ROOT" submodule update --init --recursive third_party/Cronopio || {
    echo "[build] ERROR: could not init the Cronopio submodule." >&2; exit 1; }
fi
if [[ ! -d "$UQM" ]]; then
  echo "[build] UQM submodule missing — initialising..."
  git -C "$ROOT" submodule update --init --recursive third_party/uqm || {
    echo "[build] ERROR: could not init the UQM submodule." >&2; exit 1; }
fi

SDK="$CRONOPIO/sdk"
RT="$CRONOPIO/external/CronoVM/runtime/lib"
CRBUILD="$CRONOPIO/build"
CC="$CRBUILD/tools/cronopio-cc/cronopio-cc.exe"

# Always sync tools so a fresh VM is linked into the host (avoids the
# "unknown opcode" trap a stale host throws on new opcodes). The cvm-cc that
# cronopio-cc invokes lives in the NESTED CronoVM sub-build (build/_cvm) baked
# into cronopio_cc_paths.h — build it too, or a cvm-cc change won't take effect.
if [[ ! -f "$CRBUILD/build.ninja" ]]; then
  echo "[build] configuring Cronopio SDK (one-time)..."
  cmake -S "$CRONOPIO" -B "$CRBUILD" -G Ninja || {
    echo "[build] ERROR: cmake configure of Cronopio failed." >&2; exit 1; }
fi
echo "[build] syncing Cronopio tools + host with the current VM..."
ninja -C "$CRBUILD" cronopio-cc cronopio cronopio-headless || {
  echo "[build] ERROR: building Cronopio tools failed." >&2; exit 1; }

# Build picolibc.bc — the C library (string/mem/ctype/stdlib). It is a gitignored
# artifact, rebuilt each time so it tracks the picolibc submodule + translator.
# UQM uses the Cronopio TUNED allocator (-DCRON_LIBC_TUNED_MALLOC below) as the
# CANONICAL malloc — its O(1) free beats picolibc nano-malloc's O(n) free on the
# 10559-entry ZIP content mount (~1s vs ~9s). So build picolibc WITHOUT malloc
# (cron_sys.c supplies it). DOOM/Quake keep picolibc's malloc (no --no-malloc).
echo "[build] building picolibc.bc (C library, --no-malloc --with-stdio; tuned malloc in cron_sys.c)..."
bash "$RT/build_picolibc.sh" --no-malloc --with-stdio || {
  echo "[build] ERROR: build_picolibc.sh failed." >&2; exit 1; }

OUT="${1:-$ROOT/uqm.crom}"

# Include paths: compat/ first (config_cron.h, log_cron.h shadow UQM via -I
# angle-bracket includes after our [cronopio] patches), then SDK, then UQM.
INCS=(
  -I "$ROOT/compat"
  -I "$ROOT/src"
  -I "$SDK/include"
  -I "$RT"
  -I "$US"
  -I "$US/libs"
  -I "$US/libs/uio"
  -I "$US/libs/strings"               # strintrn.h (snd_cron builds SFX STRING_TABLEs)
  -I "$US/uqm"                        # ship TUs quote-include "resinst.h" etc.
  -I "$CRONOPIO/tools/2dpak/external" # stb_image.h (PNG decode, cart-side)
  -I "$SDK/external/miniz"            # <zlib.h>/<miniz.h> (uio zip fs)
)

# ====================================================================== #
# CORE_SRCS — the WHOLE UQM game core, one find, no hand-kept KEEP lists. #
# Compile every .c under uqm/ + libs/ EXCEPT:                             #
#   - platform backends with no Cronopio equivalent: sound, the SDL/GL    #
#     graphics backends, the SDL/pthread thread backends, the SDL input   #
#     backend, network (Super Melee multiplayer), the FMV video player,   #
#     the dynamic-module loader (cdp, needs dlfcn/wtypes).                #
#   - 6 TUs that don't compile (platform-only headers: pwd/windows/signal)#
#     and the uio debug wrapper.                                          #
#   - 4 libs/* the SEAM re-implements for Cronopio (letting the stock     #
#     versions win would regress them): time/timecommon (GetTimeCounter-> #
#     cron_time_ms), memory/w_memlib (HMalloc->libc), callback/alarm +    #
#     callback/async (no-op; the cart loop doesn't pump the callback q).  #
# The SDL graphics backend dir is excluded here and re-added as           #
# GFXSDL_SRCS (compiled against compat/SDL.h, the SDL_Surface shim).      #
# ====================================================================== #
mapfile -t CORE_SRCS < <(find "$US/uqm" "$US/libs" -name '*.c' \
  ! -path '*/libs/sound/*' \
  ! -path '*/libs/graphics/sdl/*' \
  ! -path '*/libs/graphics/opengl/*' \
  ! -path '*/libs/threads/sdl/*' \
  ! -path '*/libs/threads/pthread/*' \
  ! -path '*/libs/input/sdl/*' \
  ! -path '*/libs/network/*' \
  ! -path '*/libs/video/*' \
  ! -path '*/libs/cdp/*' \
  ! -path '*/libs/file/dirs.c' \
  ! -path '*/libs/file/temp.c' \
  ! -path '*/libs/log/msgbox_win.c' \
  ! -path '*/libs/log/uqmlog.c' \
  ! -path '*/libs/uio/memdebug.c' \
  ! -path '*/libs/uio/debug.c' \
  ! -path '*/supermelee/netplay/*' \
  ! -path '*/libs/time/*' \
  ! -path '*/libs/memory/w_memlib.c' \
  ! -path '*/libs/callback/alarm.c' \
  ! -path '*/libs/callback/async.c' \
  | sort)

# port.c — strupr + other small POSIX shims (HAVE_* in compat/config_cron.h
# guard out the ones we don't want). Lives at sc2/src/port.c (outside uqm/+libs/
# so the find above misses it).
PORT_C=( "$US/port.c" )

# libs/graphics/sdl backend — UQM's real, stock canvas/primitives/palette,
# compiled against the minimal SDL_Surface shim (compat/SDL.h + src/sdl_compat.c)
# so the actual pixels land in the 32bpp screens that vid_cron downsamples to the
# 8bpp framebuffer. sdl/rotozoom.c is intentionally NOT compiled (alloca; melee
# ship rotation only) — rotateSurface/rotozoomSurfaceSize are stubbed identity.
GFXSDL_SRCS=(
  "$US/libs/graphics/sdl/canvas.c"
  "$US/libs/graphics/sdl/primitives.c"
  "$US/libs/graphics/sdl/palette.c"
)

# Our seam + cart entry + the libc + vendored miniz (zlib for uio's zip fs).
PORT=(
  "$ROOT/src/uqm_seam.c"
  "$ROOT/src/uqm_stubs_link.c"
  "$ROOT/src/sdl_compat.c"
  "$ROOT/src/img_cron.c"
  "$ROOT/src/vid_cron.c"
  "$ROOT/src/snd_cron.c"
  "$ROOT/src/main_cron.c"
  "$SDK/lib/cron_sys.c"
  "$SDK/external/miniz/miniz.c"
  "$RT/picolibc.bc"
)

echo "[build] ${#CORE_SRCS[@]} core + ${#GFXSDL_SRCS[@]} sdl-backend + ${#PORT[@]} platform TUs -> $OUT"

# -DCRONOPIO routes config.h to compat/config_cron.h and libs/log.h to
# compat/log_cron.h (both via our [cronopio] patches on the
# cronopio-port branch of the fork).
"$CC" \
  -DCRONOPIO \
  -DCRON_LIBC_TUNED_MALLOC \
  -D_POSIX_NAME_MAX=255 \
  -D_POSIX_PATH_MAX=256 \
  -DMINIZ_NO_DEFLATE_APIS -DMINIZ_NO_ARCHIVE_APIS \
  -DMINIZ_NO_STDIO -DMINIZ_NO_TIME \
  -DHAVE_ZIP \
  "${INCS[@]}" \
  "${CORE_SRCS[@]}" \
  "${PORT_C[@]}" \
  "${GFXSDL_SRCS[@]}" \
  "${PORT[@]}" \
  -o "$OUT" \
  --rom="$ROOT/content/uqm-0.8.0-content.uqm" \
  --heap-reserve=16M \
  --stack-reserve=512K \
  --title="UQM" \
  --author="Cronomantic (Cronopio port)" \
  --controls="D-pad move, A select, B cancel"
