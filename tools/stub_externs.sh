#!/usr/bin/env bash
# Iterate the build, find the first missing extern global the translator
# rejects, look up its declaration in UQM headers, append a stub
# definition to src/uqm_seam.c, and repeat. Stops when the build either
# succeeds or fails for a different reason. Limited to 100 rounds to
# avoid runaway loops.
#
# Used during the UQM port bring-up to discover the surface of external
# globals starcon.c (and friends) reference. Once the real owning TUs
# are in the KEEP list, the stubs come out.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SEAM="$ROOT/src/uqm_seam.c"
USRC="$ROOT/third_party/uqm/sc2/src"

# Marker so we can find the section we're appending to.
if ! grep -q "STUB_EXTERNS_START" "$SEAM"; then
  cat >> "$SEAM" <<'EOF'

/* ---------------------------------------------------------------------- */
/* STUB_EXTERNS_START — auto-discovered externs from tools/stub_externs.sh.
 * Each entry comes from a translator "extern not supported" error;
 * the stub gives the symbol storage so the build advances. These come
 * out as the real owning TUs land in build_uqm.sh's KEEP list. */
EOF
fi

for round in $(seq 1 100); do
  log=$(bash "$ROOT/build_uqm.sh" 2>&1 | tail -200)
  err=$(echo "$log" | grep "external global" | head -1 | sed -E "s/.*'([^']+)'.*/\1/")
  if [[ -z "$err" ]]; then
    # Either the build succeeded, or it failed for a non-extern reason.
    if echo "$log" | grep -q "cvm-translate failed\|clang failed\|cronopio-cc failed"; then
      echo "STOPPED after $((round-1)) rounds — build failed for a non-extern reason:"
      echo "$log" | tail -15
    else
      echo "BUILD GREEN after $((round-1)) stub rounds"
    fi
    break
  fi
  # Find full declaration in headers — keep first match.
  decl=$(grep -rEn "extern[[:space:]].*[^a-zA-Z_]${err}([[:space:]]|\[|;|=|,)" "$USRC" --include="*.h" | head -1)
  if [[ -z "$decl" ]]; then
    echo "round $round: $err — NO DECL FOUND, stopping"
    break
  fi
  # Pull the declaration text (after the colon), strip leading "extern ", convert decl→def.
  line=$(echo "$decl" | sed -E 's/^[^:]+:[0-9]+://')
  def=$(echo "$line" | sed -E 's/^[[:space:]]*extern[[:space:]]+//')
  # Drop any trailing comment so the appended line stays clean.
  def=$(echo "$def" | sed -E 's,/\*.*\*/,,; s,//.*$,,')
  echo "round $round: $err  ->  $def"
  echo "$def" >> "$SEAM"
done
