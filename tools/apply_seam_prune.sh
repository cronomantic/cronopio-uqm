#!/usr/bin/env bash
# Comment out file-scope DEFINITIONS in src/uqm_seam.c whose symbol appears in
# tools/_sc/seam_conflicts.txt (now core-owned). Mechanical — no eyeballing.
# Backs up to tools/_sc/uqm_seam.before.c. Run tools/seam_conflicts.sh after to
# confirm seam_conflicts dropped to 0.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SEAM="$ROOT/src/uqm_seam.c"
CONF="$ROOT/tools/_sc/seam_conflicts.txt"
cp "$SEAM" "$ROOT/tools/_sc/uqm_seam.before.c"

awk '
  NR==FNR { s=$1; if (s!="") sym[s]=1; next }
  {
    line=$0
    # leave already-commented lines alone
    if (line ~ /^[[:space:]]*\/\//) { print; next }
    hit=0; which=""
    for (s in sym) {
      # a DEFINITION: symbol immediately followed by [ , ; , = or (  (optionally
      # preceded by non-identifier char or start of line). Matches globals
      # "TYPE name;" / "TYPE name[..]" / "TYPE name =" and function "TYPE name (".
      if (line ~ ("(^|[^A-Za-z0-9_])" s "[[:space:]]*[\\[;=(]")) { hit=1; which=s; break }
    }
    if (hit) print "/* [cronopio] core owns " which " now; seam def removed: */ /* " line " */"
    else print line
  }
' "$CONF" "$ROOT/tools/_sc/uqm_seam.before.c" > "$SEAM"

echo "commented_lines=$(grep -c 'core owns' "$SEAM")"
echo "conflict_syms=$(wc -l < "$CONF")"
