#!/usr/bin/env bash
# ONE-PASS bitcode measurement of the WHOLE UQM game core.
# Discipline ([[uqm-compile-core-not-stub]]): compile EVERYTHING under uqm/ and
# libs/ EXCEPT the platform backends (sound, the SDL graphics/threads/input
# backends, network, video player). The undefined residual after linking all
# the compilable TUs IS the platform seam — that, and only that, gets stubbed.
#
# Output (tools/_core/):
#   compile_ok.txt    TUs that compiled to bitcode (game core — COMPILE these)
#   compile_fail.txt  TUs that did NOT compile (header gaps / translator gaps)
#   fail_reason.txt   first error line per failing TU (gap classification)
#   residual.txt      undefined symbols after linking all OK bitcode = the SEAM
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UQM="$ROOT/third_party/uqm/sc2"; US="$UQM/src"
CRONOPIO="$ROOT/third_party/Cronopio"; SDK="$CRONOPIO/sdk"
RT="$CRONOPIO/external/CronoVM/runtime/lib"
BIN="/c/Users/Sergio/scoop/apps/mingw-mstorsjo-llvm-ucrt/current/bin"
CLANG="$BIN/clang.exe"
NM="/c/msys64/ucrt64/bin/llvm-nm.exe"
LINK="/c/msys64/ucrt64/bin/llvm-link.exe"
OUT="$ROOT/tools/_core"; rm -rf "$OUT"; mkdir -p "$OUT/bc"

INCS=(-I "$ROOT/compat" -I "$ROOT/src" -I "$SDK/include" -I "$RT"
      -I "$US" -I "$US/libs" -I "$US/libs/uio" -I "$US/uqm"
      -I "$CRONOPIO/tools/2dpak/vendor")
DEFS=(-DCRONOPIO -DCRON_LIBC_TUNED_MALLOC -D_POSIX_NAME_MAX=255
      -D_POSIX_PATH_MAX=256 -DMINIZ_NO_DEFLATE_APIS -DMINIZ_NO_ARCHIVE_APIS
      -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -DHAVE_ZIP)
CFLAGS=(--target=i386-elf -ffreestanding -emit-llvm -O1 -c -Wno-everything
        -gline-tables-only)

# All .c under uqm/ and libs/, MINUS the platform backends.
mapfile -t ALL < <(find "$US/uqm" "$US/libs" -name '*.c' \
  ! -path '*/libs/sound/*' \
  ! -path '*/libs/graphics/sdl/*' \
  ! -path '*/libs/graphics/opengl/*' \
  ! -path '*/libs/threads/sdl/*' \
  ! -path '*/libs/threads/pthread/*' \
  ! -path '*/libs/input/sdl/*' \
  ! -path '*/libs/network/*' \
  ! -path '*/libs/video/*' \
  ! -path '*/supermelee/netplay/*' \
  | sort)

: > "$OUT/compile_ok.txt"; : > "$OUT/compile_fail.txt"; : > "$OUT/fail_reason.txt"
BCS=()
for f in "${ALL[@]}"; do
  rel="${f#$US/}"
  bc="$OUT/bc/$(echo "$rel" | tr '/' '_').bc"
  if "$CLANG" "${CFLAGS[@]}" "${DEFS[@]}" "${INCS[@]}" "$f" -o "$bc" 2> "$bc.err"; then
    echo "$rel" >> "$OUT/compile_ok.txt"; BCS+=("$bc")
  else
    echo "$rel" >> "$OUT/compile_fail.txt"
    printf '%-50s %s\n' "$rel" "$(grep -m1 'error:' "$bc.err" | sed 's/.*error: //')" >> "$OUT/fail_reason.txt"
  fi
done

echo "OK=$(wc -l < "$OUT/compile_ok.txt")  FAIL=$(wc -l < "$OUT/compile_fail.txt")" > "$OUT/summary.txt"

# Link all OK bitcode (+ the cart's own seam-free game bitcode is implicit via
# these TUs). Undefined symbols = what no game TU provides = the seam.
if [ "${#BCS[@]}" -gt 0 ]; then
  "$LINK" "${BCS[@]}" -o "$OUT/core_all.bc" 2> "$OUT/link.err" \
    && "$NM" "$OUT/core_all.bc" 2>&1 | awk '$1=="U"||$2=="U"{print $NF}' | sort -u > "$OUT/residual.txt"
  echo "RESIDUAL=$(wc -l < "$OUT/residual.txt" 2>/dev/null || echo NA)" >> "$OUT/summary.txt"
fi
cat "$OUT/summary.txt"
