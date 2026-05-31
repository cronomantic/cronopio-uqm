#!/usr/bin/env bash
# Recompile ONLY uqm_seam.c + uqm_stubs_link.c and re-check conflicts against the
# already-computed tools/_sc/other_defs.txt. Fast (2 TUs). Writes a summary file.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UQM="$ROOT/third_party/uqm/sc2"; US="$UQM/src"
CRONOPIO="$ROOT/third_party/Cronopio"; SDK="$CRONOPIO/sdk"
RT="$CRONOPIO/external/CronoVM/runtime/lib"
BIN="/c/Users/Sergio/scoop/apps/mingw-mstorsjo-llvm-ucrt/current/bin"
CLANG="$BIN/clang.exe"; NM="/c/msys64/ucrt64/bin/llvm-nm.exe"
O="$ROOT/tools/_sc"
INCS=(-I "$ROOT/compat" -I "$ROOT/src" -I "$SDK/include" -I "$RT"
      -I "$US" -I "$US/libs" -I "$US/libs/uio" -I "$US/uqm"
      -I "$CRONOPIO/tools/2dpak/vendor")
DEFS=(-DCRONOPIO -DCRON_LIBC_TUNED_MALLOC -D_POSIX_NAME_MAX=255
      -D_POSIX_PATH_MAX=256 -DMINIZ_NO_DEFLATE_APIS -DMINIZ_NO_ARCHIVE_APIS
      -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -DHAVE_ZIP)
CF=(--target=i386-elf -ffreestanding -emit-llvm -O1 -c -Wno-everything)

rm -f "$O/v_seam.bc" "$O/v_stubs.bc"
"$CLANG" "${CF[@]}" "${DEFS[@]}" "${INCS[@]}" "$ROOT/src/uqm_seam.c"       -o "$O/v_seam.bc"  2>"$O/v_seam.err";  seam_ok=$?
"$CLANG" "${CF[@]}" "${DEFS[@]}" "${INCS[@]}" "$ROOT/src/uqm_stubs_link.c" -o "$O/v_stubs.bc" 2>"$O/v_stubs.err"; stubs_ok=$?

> "$O/v_seam_conf.txt"; > "$O/v_stubs_conf.txt"
[ -f "$O/v_seam.bc" ]  && "$NM" --defined-only "$O/v_seam.bc"  2>/dev/null | awk '{print $NF}' | sort -u | comm -12 - "$O/other_defs.txt" > "$O/v_seam_conf.txt"
[ -f "$O/v_stubs.bc" ] && "$NM" --defined-only "$O/v_stubs.bc" 2>/dev/null | awk '{print $NF}' | sort -u | comm -12 - "$O/other_defs.txt" > "$O/v_stubs_conf.txt"

{
  echo "seam_compile_rc=$seam_ok stubs_compile_rc=$stubs_ok"
  echo "seam_remaining_conflicts=$(wc -l < "$O/v_seam_conf.txt")"
  echo "stubs_remaining_conflicts=$(wc -l < "$O/v_stubs_conf.txt")"
} > "$O/verify.txt"
cat "$O/verify.txt"
