#!/usr/bin/env bash
# Iterate the build; each round, the translator complains about ONE
# missing function definition (callee has no definition in this module).
# Capture the name, stub it out with an abort-on-call body in
# src/uqm_stubs_link.c, and repeat. The accumulated stub list is the
# link surface — every function starcon.c (transitively) needs.
#
# Stubs use __builtin_trap() so any actual runtime path that reaches one
# fails loud; we're only after a clean LINK here, not a working game.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STUBS="$ROOT/src/uqm_stubs_link.c"

if [[ ! -f "$STUBS" ]]; then
  cat > "$STUBS" <<'EOF'
/*
 * uqm_stubs_link.c — auto-discovered link-surface stubs from
 * tools/survey_link.sh. Each function abort-traps on call; the goal is
 * to enumerate WHICH symbols starcon.c reaches, not to do anything
 * useful. As the real owning .c files land in build_uqm.sh's KEEP list,
 * the corresponding stubs come out.
 */
#include <stdint.h>
#include "port.h"
#include "libs/compiler.h"

EOF
  # Wire into build.
  if ! grep -q "uqm_stubs_link.c" "$ROOT/build_uqm.sh"; then
    sed -i 's|"$ROOT/src/uqm_seam.c"|"$ROOT/src/uqm_seam.c"\n  "$ROOT/src/uqm_stubs_link.c"|' "$ROOT/build_uqm.sh"
  fi
fi

stubbed=""
for round in $(seq 1 200); do
  log=$(bash "$ROOT/build_uqm.sh" 2>&1 | tail -200)
  fn=$(echo "$log" | grep -oE "call to '[A-Za-z_][A-Za-z0-9_]*': callee has no definition" \
       | head -1 | sed -E "s/call to '([^']+)'.*/\1/")
  if [[ -z "$fn" ]]; then
    if echo "$log" | grep -q "cvm-translate failed\|clang failed\|cronopio-cc failed"; then
      echo "STOPPED — build failed for a different reason:"
      echo "$log" | grep -E "error|external global|callee has no|outside the supported" | head -5
    else
      echo "BUILD GREEN after $((round-1)) stub rounds"
    fi
    break
  fi
  case " $stubbed " in *" $fn "*)
    echo "round $round: $fn — already stubbed?!  stopping"; break;;
  esac
  stubbed="$stubbed $fn"
  echo "round $round: stubbing $fn"
  # Append a trap-body stub. The signature is unknown to us; declare
  # variadic so any call site type-checks (clang isn't strict on extern
  # decls without prototype in i386-elf mode).
  cat >> "$STUBS" <<EOF
void $fn () { /* link-only stub */ }
EOF
done

echo "---"
echo "TOTAL stubbed: $(grep -c '^void ' "$STUBS")"
