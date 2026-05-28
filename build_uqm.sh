#!/usr/bin/env bash
# Build the Cronopio UQM spike cartridge.
#
# For now this is just the THREADING SPIKE: compile libs/threads/cron +
# libs/task + the small UQM deps + our seam + a stub Starcon2Main, link
# into uqm.crom, and run it on cronopio-headless. If three coroutine
# "threads" make progress without deadlock, the spike succeeds.
#
# Once the spike is green, this script will grow KEEP lists like
# build_doom.sh / build_quake.sh and start compiling actual UQM subsystems.
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
# "unknown opcode" trap a stale host throws on new opcodes — we just
# landed CORO_SWAP, so this matters here).
if [[ ! -f "$CRBUILD/build.ninja" ]]; then
  echo "[build] configuring Cronopio SDK (one-time)..."
  cmake -S "$CRONOPIO" -B "$CRBUILD" -G Ninja || {
    echo "[build] ERROR: cmake configure of Cronopio failed." >&2; exit 1; }
fi
echo "[build] syncing Cronopio tools + host with the current VM..."
ninja -C "$CRBUILD" cronopio-cc cronopio cronopio-headless || {
  echo "[build] ERROR: building Cronopio tools failed." >&2; exit 1; }

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
)

# UQM translation units we compile this round. Keep MINIMAL — just enough
# to exercise the threading layer. Grow as new subsystems land.
UQM_SRCS=(
  "$US/libs/threads/cron/cronthreads.c"
  "$US/libs/threads/thrcommon.c"
  "$US/libs/task/tasklib.c"
  "$US/uqm/starcon.c"
)

# Our seam + cart entry + the libc.
PORT=(
  "$ROOT/src/uqm_seam.c"
  "$ROOT/src/uqm_stubs_link.c"
  "$ROOT/src/main_cron.c"
  "$SDK/lib/cvm_libc.c"
)

echo "[build] $(( ${#UQM_SRCS[@]} + ${#PORT[@]} )) translation units -> $OUT"

# -DCRONOPIO routes config.h to compat/config_cron.h and libs/log.h to
# compat/log_cron.h (both via our [cronopio] patches on the
# cronopio-port branch of the fork).
"$CC" \
  -DCRONOPIO \
  "${INCS[@]}" \
  "${UQM_SRCS[@]}" \
  "${PORT[@]}" \
  --heap-reserve=2M \
  --stack-reserve=512K \
  --title="UQM spike" \
  --author="Cronomantic (Cronopio port)" \
  --controls="N/A (threading spike — no UI yet)" \
  -o "$OUT"
