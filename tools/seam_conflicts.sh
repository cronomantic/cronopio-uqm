#!/usr/bin/env bash
# Compute the EXACT set of symbols that uqm_seam.c / uqm_stubs_link.c define AND
# some OTHER build TU also defines (= llvm-link multiply-defined). Writes results
# to tools/_sc/ as one-symbol-per-line files (the shell truncates multi-line
# stdout this session, but the Read tool reads files fine).
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UQM="$ROOT/third_party/uqm/sc2"; US="$UQM/src"
CRONOPIO="$ROOT/third_party/Cronopio"; SDK="$CRONOPIO/sdk"
RT="$CRONOPIO/external/CronoVM/runtime/lib"
BIN="/c/Users/Sergio/scoop/apps/mingw-mstorsjo-llvm-ucrt/current/bin"
CLANG="$BIN/clang.exe"; NM="/c/msys64/ucrt64/bin/llvm-nm.exe"
O="$ROOT/tools/_sc"; rm -rf "$O"; mkdir -p "$O/bc"

INCS=(-I "$ROOT/compat" -I "$ROOT/src" -I "$SDK/include" -I "$RT"
      -I "$US" -I "$US/libs" -I "$US/libs/uio" -I "$US/uqm"
      -I "$CRONOPIO/tools/2dpak/vendor")
DEFS=(-DCRONOPIO -DCRON_LIBC_TUNED_MALLOC -D_POSIX_NAME_MAX=255
      -D_POSIX_PATH_MAX=256 -DMINIZ_NO_DEFLATE_APIS -DMINIZ_NO_ARCHIVE_APIS
      -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -DHAVE_ZIP)
CF=(--target=i386-elf -ffreestanding -emit-llvm -O1 -c -Wno-everything)

# Same find as build_uqm.sh
mapfile -t CORE < <(find "$US/uqm" "$US/libs" -name '*.c' \
  ! -path '*/libs/sound/*' ! -path '*/libs/graphics/sdl/*' ! -path '*/libs/graphics/opengl/*' \
  ! -path '*/libs/threads/sdl/*' ! -path '*/libs/threads/pthread/*' ! -path '*/libs/input/sdl/*' \
  ! -path '*/libs/network/*' ! -path '*/libs/video/*' ! -path '*/libs/cdp/*' \
  ! -path '*/libs/file/dirs.c' ! -path '*/libs/file/temp.c' ! -path '*/libs/log/msgbox_win.c' \
  ! -path '*/libs/log/uqmlog.c' ! -path '*/libs/uio/memdebug.c' ! -path '*/supermelee/netplay/*' \
  ! -path '*/libs/time/timecommon.c' ! -path '*/libs/memory/w_memlib.c' \
  ! -path '*/libs/callback/alarm.c' ! -path '*/libs/callback/async.c' | sort)

bc_of() { echo "$O/bc/$(echo "$1" | tr '/:\\' '___').bc"; }

# OTHER = everything in the build EXCEPT uqm_seam.c + uqm_stubs_link.c.
: > "$O/other_defs.txt"
fail=0
for f in "${CORE[@]}" "$US/port.c" \
         "$US/libs/graphics/sdl/canvas.c" "$US/libs/graphics/sdl/primitives.c" "$US/libs/graphics/sdl/palette.c" \
         "$ROOT/src/sdl_compat.c" "$ROOT/src/img_cron.c" "$ROOT/src/vid_cron.c" "$ROOT/src/main_cron.c" \
         "$SDK/lib/cron_sys.c" "$SDK/lib/miniz.c"; do
  b="$(bc_of "$f")"
  if "$CLANG" "${CF[@]}" "${DEFS[@]}" "${INCS[@]}" "$f" -o "$b" 2>"$b.err"; then
    "$NM" --defined-only "$b" 2>/dev/null | awk '{print $NF}' >> "$O/other_defs.txt"
  else
    echo "$f" >> "$O/compile_fail.txt"; fail=$((fail+1))
  fi
done
# picolibc defs too
"$NM" --defined-only "$RT/picolibc.bc" 2>/dev/null | awk '{print $NF}' >> "$O/other_defs.txt"
sort -u "$O/other_defs.txt" -o "$O/other_defs.txt"

# seam + stubs defs
"$CLANG" "${CF[@]}" "${DEFS[@]}" "${INCS[@]}" "$ROOT/src/uqm_seam.c" -o "$O/seam.bc" 2>"$O/seam.err" \
  && "$NM" --defined-only "$O/seam.bc" 2>/dev/null | awk '{print $NF}' | sort -u > "$O/seam_defs.txt"
"$CLANG" "${CF[@]}" "${DEFS[@]}" "${INCS[@]}" "$ROOT/src/uqm_stubs_link.c" -o "$O/stubs.bc" 2>"$O/stubs.err" \
  && "$NM" --defined-only "$O/stubs.bc" 2>/dev/null | awk '{print $NF}' | sort -u > "$O/stubs_defs.txt"

comm -12 "$O/seam_defs.txt"  "$O/other_defs.txt" > "$O/seam_conflicts.txt"  2>/dev/null
comm -12 "$O/stubs_defs.txt" "$O/other_defs.txt" > "$O/stubs_conflicts.txt" 2>/dev/null

{
  echo "core_TUs=${#CORE[@]} other_compile_fail=$fail"
  echo "other_defs=$(wc -l < "$O/other_defs.txt")"
  echo "seam_defs=$(wc -l < "$O/seam_defs.txt" 2>/dev/null) seam_conflicts=$(wc -l < "$O/seam_conflicts.txt" 2>/dev/null)"
  echo "stubs_defs=$(wc -l < "$O/stubs_defs.txt" 2>/dev/null) stubs_conflicts=$(wc -l < "$O/stubs_conflicts.txt" 2>/dev/null)"
  echo "seam_compile_ok=$([ -f "$O/seam.bc" ] && echo yes || echo NO)"
  echo "stubs_compile_ok=$([ -f "$O/stubs.bc" ] && echo yes || echo NO)"
} > "$O/summary.txt"
cat "$O/summary.txt"
