#!/usr/bin/env bash
# Link the full build (reusing tools/_sc/bc + pruned seam + current stubs +
# picolibc) and write the genuinely-undefined symbols to tools/_sc/missing.txt.
# llvm-link only errors on DUP defs (seam is now conflict-free), so it produces
# the module even with undefined refs. Then split missing into:
#   - has_owner.txt : some .c defines it -> a TU is missing from the build (BUG)
#   - noowner.txt   : no .c defines it   -> genuine seam to stub
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UQM="$ROOT/third_party/uqm/sc2"; US="$UQM/src"
CRONOPIO="$ROOT/third_party/Cronopio"; SDK="$CRONOPIO/sdk"
RT="$CRONOPIO/external/CronoVM/runtime/lib"
BIN="/c/Users/Sergio/scoop/apps/mingw-mstorsjo-llvm-ucrt/current/bin"
CLANG="$BIN/clang.exe"; NM="/c/msys64/ucrt64/bin/llvm-nm.exe"; LINK="/c/msys64/ucrt64/bin/llvm-link.exe"
O="$ROOT/tools/_sc"
INCS=(-I "$ROOT/compat" -I "$ROOT/src" -I "$SDK/include" -I "$RT"
      -I "$US" -I "$US/libs" -I "$US/libs/uio" -I "$US/uqm"
      -I "$CRONOPIO/tools/2dpak/vendor")
DEFS=(-DCRONOPIO -DCRON_LIBC_TUNED_MALLOC -D_POSIX_NAME_MAX=255
      -D_POSIX_PATH_MAX=256 -DMINIZ_NO_DEFLATE_APIS -DMINIZ_NO_ARCHIVE_APIS
      -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -DHAVE_ZIP)
CF=(--target=i386-elf -ffreestanding -emit-llvm -O1 -c -Wno-everything)

# (re)compile pruned seam + current stubs
"$CLANG" "${CF[@]}" "${DEFS[@]}" "${INCS[@]}" "$ROOT/src/uqm_seam.c"       -o "$O/L_seam.bc"  2>"$O/L_seam.err"  || { echo "SEAM COMPILE FAIL"; exit 1; }
"$CLANG" "${CF[@]}" "${DEFS[@]}" "${INCS[@]}" "$ROOT/src/uqm_stubs_link.c" -o "$O/L_stubs.bc" 2>"$O/L_stubs.err" || { echo "STUBS COMPILE FAIL"; cat "$O/L_stubs.err"|head; exit 1; }

# all build .bc = the OTHER set in _sc/bc (core+port+sdl+cart-platform) MINUS the
# seam/stubs bc that script wrote (it didn't write those), plus our fresh two.
LBC=()
for b in "$O"/bc/*.bc; do
  case "$(basename "$b")" in *uqm_seam*|*uqm_stubs_link*) continue;; esac
  LBC+=("$b")
done
LBC+=("$O/L_seam.bc" "$O/L_stubs.bc" "$RT/picolibc.bc")
"$LINK" "${LBC[@]}" -o "$O/full.bc" 2>"$O/L_link.err"
echo "link_rc=$? bc_count=${#LBC[@]}" > "$O/missing_summary.txt"
head -3 "$O/L_link.err" >> "$O/missing_summary.txt"

if [ -f "$O/full.bc" ]; then
  "$NM" --undefined-only "$O/full.bc" 2>/dev/null | awk '{print $NF}' | tr -d '\r' | sort -u \
    | grep -vE '^cvm_sys_|^_GLOBAL_OFFSET_TABLE_$|^__cvm_coro_swap_raw$|^__math_' > "$O/missing.txt"
  : > "$O/noowner.txt"; : > "$O/has_owner.txt"
  while read -r s; do
    [ -z "$s" ] && continue
    if grep -rlqE "^[A-Za-z].*[^A-Za-z0-9_]${s}[[:space:]]*\(" "$US" --include=*.c 2>/dev/null; then
      echo "$s" >> "$O/has_owner.txt"
    else
      echo "$s" >> "$O/noowner.txt"
    fi
  done < "$O/missing.txt"
  {
    echo "missing_total=$(wc -l < "$O/missing.txt")"
    echo "noowner_to_stub=$(wc -l < "$O/noowner.txt")"
    echo "has_owner_BUG=$(wc -l < "$O/has_owner.txt")"
  } >> "$O/missing_summary.txt"
fi
cat "$O/missing_summary.txt"
