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
  -I "$CRONOPIO/tools/2dpak/vendor"   # stb_image.h (PNG decode, cart-side)
)

# UQM translation units we compile this round. Keep MINIMAL — just enough
# to exercise the threading layer. Grow as new subsystems land.
UQM_SRCS=(
  "$US/libs/threads/cron/cronthreads.c"
  "$US/libs/threads/thrcommon.c"
  "$US/libs/task/tasklib.c"
  "$US/uqm/starcon.c"
  "$US/uqm/globdata.c"
  "$US/uqm/displist.c"
  "$US/uqm/master.c"
  "$US/uqm/setup.c"
  # GFX slice 4c: the real UQM input pipeline (pad -> ImmediateInputState ->
  # UpdateInputState -> PulsedInputState).
  "$US/uqm/gameinp.c"
  # GFX slice 4d: the real main-menu state machine (restart.c) + the flash
  # overlay (the pulsing highlight). Leaf activities reached only on a menu
  # selection (Melee/Credits/Introduction/SetupMenu/save-load) stay stubbed.
  "$US/uqm/restart.c"
  "$US/uqm/flash.c"
  # Slice 5a: the NEW-GAME init chain. InitGameStructures (globdata.c, already
  # above) drives InitSISContexts/DrawSISFrame (border.c) + planet/group/build
  # data; clock.c/gameev.c own the game clock + initial events; cleanup.c owns
  # UninitGameKernel. ExploreSolarSys/Battle stay stubbed (slice 5b). Goal:
  # New Game runs the real init + draws the SIS frame, no trap.
  "$US/uqm/clock.c"
  "$US/uqm/gameev.c"
  "$US/uqm/cleanup.c"
  "$US/uqm/border.c"
  "$US/uqm/state.c"
  "$US/uqm/grpinfo.c"
  "$US/uqm/build.c"
  "$US/uqm/loadship.c"
  # battlecontrols.c owns HumanInputContext_new/ComputerInputContext_new (+ the
  # real PlayerInput[]); SetPlayerInputAll needs them to return non-NULL contexts
  # or the new game aborts (explode). The battle/AI frameInput handlers it refs
  # (computer_intelligence/frameInputHuman/select*/battleEndReady*) stay stubbed —
  # only called during battle (slice 5b).
  "$US/uqm/battlecontrols.c"
)

# Slice 5b (step 1): populate the MASTER SHIP LIST. dummy.c is the "code
# resource" type handler — it registers the "SHIP" resource type whose loader
# maps a ship id -> init_<ship>() (e.g. ARILOU_CODE -> init_arilou), each of
# which returns that ship's static RACE_DESC. LoadMasterShipList (master.c)
# calls load_ship (loadship.c) -> LoadCodeRes -> dummy.c for all 25 meleeable
# ships, building master_q. dummy.c's switch hard-references all 28 init_*
# symbols, so every ship TU must LINK. We compile them with LoadBattleData=
# FALSE at load time, so only init_<ship> + the static RACE_DESC + the
# icon/string resources are exercised; each ship's battle AI/preprocess/weapon
# functions reference the combat engine (intel/weapon/collide/element) but are
# NEVER called during master-list build — those combat symbols are link-only
# stubs (slice 5b battle proper). Quote-includes ("resinst.h"/"icode.h")
# resolve to each ship's own dir, so no per-ship -I is needed.
SHIP_SRCS=(
  "$US/uqm/dummy.c"
  "$US/uqm/ships/androsyn/androsyn.c"
  "$US/uqm/ships/arilou/arilou.c"
  "$US/uqm/ships/blackurq/blackurq.c"
  "$US/uqm/ships/chenjesu/chenjesu.c"
  "$US/uqm/ships/chmmr/chmmr.c"
  "$US/uqm/ships/druuge/druuge.c"
  "$US/uqm/ships/human/human.c"
  "$US/uqm/ships/ilwrath/ilwrath.c"
  "$US/uqm/ships/lastbat/lastbat.c"
  "$US/uqm/ships/melnorme/melnorme.c"
  "$US/uqm/ships/mmrnmhrm/mmrnmhrm.c"
  "$US/uqm/ships/mycon/mycon.c"
  "$US/uqm/ships/orz/orz.c"
  "$US/uqm/ships/pkunk/pkunk.c"
  "$US/uqm/ships/probe/probe.c"
  "$US/uqm/ships/shofixti/shofixti.c"
  "$US/uqm/ships/sis_ship/sis_ship.c"
  "$US/uqm/ships/slylandr/slylandr.c"
  "$US/uqm/ships/spathi/spathi.c"
  "$US/uqm/ships/supox/supox.c"
  "$US/uqm/ships/syreen/syreen.c"
  "$US/uqm/ships/thradd/thradd.c"
  "$US/uqm/ships/umgah/umgah.c"
  "$US/uqm/ships/urquan/urquan.c"
  "$US/uqm/ships/utwig/utwig.c"
  "$US/uqm/ships/vux/vux.c"
  "$US/uqm/ships/yehat/yehat.c"
  "$US/uqm/ships/zoqfot/zoqfot.c"
)

# Slice 5b: the INTERPLANETARY solar-system view (the real ExploreSolarSys).
# solarsys.c is the view itself; planets.c/orbits.c/calc.c/oval.c place + draw
# the sun, planets and their orbit ellipses; process.c is the ELEMENT display
# list (the flagship + planets are display elements; also provides AllocElement/
# RedrawQueue, shared with battle); ipdisp.c drives the interplanetary display;
# sis.c draws the SIS status panel (the flagship cutaway + crew/fuel gauges);
# port.c supplies strupr (sis.c uses it); libs/math/random2.c is the RNG context
# the planet code seeds with; libs/graphics/intersec.c is DrawablesIntersect/
# BoxIntersect. Deeper features reachable FROM the view but only by player
# action — planet scan/lander/surface generation (scan/lander/pl_stuff/plangen/
# generate), the SIS menus (cargo/devices/roster), the star map (pstarmap/
# starmap) and game options (menu/gameopt) — stay boundary-stubbed until their
# own slices.
IP_SRCS=(
  "$US/uqm/planets/solarsys.c"
  "$US/uqm/planets/planets.c"
  "$US/uqm/planets/orbits.c"
  "$US/uqm/planets/calc.c"
  "$US/uqm/planets/oval.c"
  "$US/uqm/sis.c"
  "$US/uqm/ipdisp.c"
  "$US/uqm/process.c"
  "$US/port.c"
  "$US/uqm/trans.c"                # ARCTAN + sinetab — the angle/sine math the
                                   # whole game uses (SINE/COSINE/orbital angles);
                                   # a clean leaf (no deps). Mis-stubbing ARCTAN +
                                   # the empty sinetab gave garbage planet frames →
                                   # DrawStamp drew a garbage-sized stamp = hang.
  "$US/libs/math/sqrt.c"           # square_root — general math leaf (9 callers); a
                                   # garbage-returning stub is an ARCTAN-class landmine.
  "$US/libs/math/random.c"         # TFB_Random/TFB_SeedRandom — the general RNG (16
                                   # callers: ships, star background, planet seeds);
                                   # garbage RNG = another ARCTAN-class landmine.
  "$US/uqm/velocity.c"             # velocity-vector math — the flagship's INTERPLANETARY
                                   # inertial movement uses it too (not battle-only);
                                   # a clean leaf (needs only ARCTAN+sinetab from trans.c).
  "$US/libs/math/random2.c"
  "$US/libs/graphics/intersec.c"
  "$US/libs/graphics/boxint.c"     # BoxIntersect/BoxUnion (clean geometry leaf)
  "$US/libs/graphics/clipline.c"   # _clip_line (clean line-clip leaf, used by oval)
  "$US/uqm/plandata.c"             # the galaxy: starmap_array (FindStar) + element/planet tables
  "$US/uqm/starmap.c"              # FindStar/GetClusterName/CurStarDescPtr/star_array (locate the system)
)

# Slice 5b cont.: PLANET GENERATION — populate each solar system with its planets.
# gendef.c's getGenerateFunctions maps a star Index -> that system's generator
# table (Sol -> generateSolFunctions); it hard-references all ~29 generate*Functions
# tables, so EVERY planets/generate/*.c must link (same hard-ref pattern as
# ships+dummy.c). gendefault.c is the shared default generator the per-system ones
# fall back to. These place the planets (generatePlanets); the deeper planet-SURFACE
# engine (gentopo/plangen/surface/pl_stuff — needs libm exp/acos) and the scan/
# lander/life/mineral INTERACTION reached only on planet orbit/scan stay boundary-
# stubbed for now.
GEN_SRCS=(
  "$US/uqm/gendef.c"
  "$US/uqm/planets/generate/gendefault.c"
  "$US/uqm/planets/generate/genand.c"
  "$US/uqm/planets/generate/genburv.c"
  "$US/uqm/planets/generate/genchmmr.c"
  "$US/uqm/planets/generate/gencol.c"
  "$US/uqm/planets/generate/gendru.c"
  "$US/uqm/planets/generate/genilw.c"
  "$US/uqm/planets/generate/genmel.c"
  "$US/uqm/planets/generate/genmyc.c"
  "$US/uqm/planets/generate/genorz.c"
  "$US/uqm/planets/generate/genpet.c"
  "$US/uqm/planets/generate/genpku.c"
  "$US/uqm/planets/generate/genrain.c"
  "$US/uqm/planets/generate/gensam.c"
  "$US/uqm/planets/generate/genshof.c"
  "$US/uqm/planets/generate/gensly.c"
  "$US/uqm/planets/generate/gensol.c"
  "$US/uqm/planets/generate/genspa.c"
  "$US/uqm/planets/generate/gensup.c"
  "$US/uqm/planets/generate/gensyr.c"
  "$US/uqm/planets/generate/genthrad.c"
  "$US/uqm/planets/generate/gentrap.c"
  "$US/uqm/planets/generate/genutw.c"
  "$US/uqm/planets/generate/genvault.c"
  "$US/uqm/planets/generate/genvux.c"
  "$US/uqm/planets/generate/genwreck.c"
  "$US/uqm/planets/generate/genyeh.c"
  "$US/uqm/planets/generate/genzfpscout.c"
  "$US/uqm/planets/generate/genzoq.c"
)

# Slice 5b cont.: PLANET-SURFACE engine — ORBIT + SCAN a world. pl_stuff (the
# rotating planet sphere) + plangen (elevation/topology → needs libm exp/acos,
# now in picolibc) + gentopo (topography) + surface (mineral/energy/life node
# generation) + scan (the scan screen + node retrieval). Compiling these lets a
# generator's generateOrbital/generateMinerals/... hooks run for real when the
# flagship enters a planet's orbit. The LANDER minigame (lander.c) + the
# discovery report (report.c) stay boundary-stubbed (deeper than scan).
SURF_SRCS=(
  "$US/uqm/planets/pl_stuff.c"
  "$US/uqm/planets/plangen.c"
  "$US/uqm/planets/gentopo.c"
  "$US/uqm/planets/surface.c"
  "$US/uqm/planets/scan.c"
  "$US/uqm/cons_res.c"
)

# libs/uio — the faithful UQM virtual filesystem (reads the .uqm content
# packs, which are ZIPs). Bring-up in progress: backed on our libc stdio +
# cron_rom; the zip layer needs a zlib inflate (TBD). Listed separately so
# the gap-discovery loops can churn through it.
UIO_SRCS=(
  "$US/libs/uio/charhashtable.c"
  "$US/libs/uio/defaultfs.c"
  "$US/libs/uio/fileblock.c"
  "$US/libs/uio/fstypes.c"
  "$US/libs/uio/gphys.c"
  "$US/libs/uio/hashtable.c"
  "$US/libs/uio/io.c"
  "$US/libs/uio/ioaux.c"
  "$US/libs/uio/match.c"
  "$US/libs/uio/mount.c"
  "$US/libs/uio/mounttree.c"
  "$US/libs/uio/paths.c"
  "$US/libs/uio/physical.c"
  "$US/libs/uio/uiostream.c"
  "$US/libs/uio/uioutils.c"
  "$US/libs/uio/utils.c"
  "$US/libs/uio/stdio/stdio.c"   # WIP: needs <dirent.h> (opendir/readdir)
                                 #      backed by the RAM-FS / cron_rom
  "$US/libs/uio/zip/zip.c"       # WIP: needs a zlib inflate
  "$US/libs/file/files.c"
  # dirs.c / temp.c — POSIX user home/config/temp dir resolution (pwd.h,
  # getpwuid). The cart hand-sets content/config/save paths via the seam,
  # so these are intentionally NOT compiled.
  # "$US/libs/file/dirs.c"
  # "$US/libs/file/temp.c"
)

# libs/resource — the resource system: parses the .rmp resource index
# (PropFile) into a name->{type,path} map over uio, and loads resources by
# name. The graphic/audio/video TYPE handlers it installs are link symbols
# (stubbed) until those subsystems land — registering them does not call them.
RES_SRCS=(
  "$US/libs/resource/direct.c"
  "$US/libs/resource/filecntl.c"
  "$US/libs/resource/getres.c"
  "$US/libs/resource/loadres.c"
  "$US/libs/resource/propfile.c"
  "$US/libs/resource/resinit.c"
  "$US/libs/resource/stringbank.c"
)

# libs/strings — string tables + the STRTAB/BINTAB/CONVERSATION resource type
# handlers (InstallStringTableResType). Lets BINTAB/STRTAB resources resolve
# and load (colormaps, race strings, etc.). Plain data parsing over uio; no
# graphics. Unicode helpers included for the UTF-8 string ops.
STR_SRCS=(
  "$US/libs/strings/getstr.c"
  "$US/libs/strings/sfileins.c"
  "$US/libs/strings/sresins.c"
  "$US/libs/strings/stringhashtable.c"
  "$US/libs/strings/strings.c"
  "$US/libs/strings/unicode.c"
)

# libs/graphics core — the platform-independent context/drawable/frame layer
# + gfx_common globals (ScreenWidth/Height live here) + the TFB front-end:
# tfb_draw.c / tfb_prim.c enqueue draw commands; dcqueue.c drains them and
# calls the backend's TFB_DrawCanvas_*; bbox.c tracks dirty regions.
GFX_SRCS=(
  "$US/libs/graphics/gfx_common.c"
  "$US/libs/graphics/context.c"
  "$US/libs/graphics/drawable.c"
  "$US/libs/graphics/frame.c"
  "$US/libs/graphics/pixmap.c"
  "$US/libs/graphics/cmap.c"
  "$US/libs/graphics/tfb_draw.c"
  "$US/libs/graphics/tfb_prim.c"
  "$US/libs/graphics/dcqueue.c"
  "$US/libs/graphics/bbox.c"
  # GFX slice 3: cel/font resource loaders. resgfx installs the GFXRES/FONTRES
  # type handlers; gfxload decodes .ani cel lists + font dirs (PNGs via
  # sdluio_loadImage -> stb); font.c is the text/_text_blt layer; loaddisp +
  # filegfx round out the graphic-resource I/O.
  "$US/libs/graphics/resgfx.c"
  "$US/libs/graphics/gfxload.c"
  "$US/libs/graphics/font.c"
  "$US/libs/graphics/loaddisp.c"
  "$US/libs/graphics/filegfx.c"
)

# libs/graphics/sdl backend — GFX slice 2: UQM's real, stock canvas/primitives/
# rotozoom rendering, compiled against the minimal SDL_Surface shim
# (compat/SDL.h + src/sdl_compat.c). These push the actual pixels into the
# 32bpp screen canvases, which vid_cron downsamples to the 8bpp framebuffer.
# NOTE: sdl/rotozoom.c is intentionally NOT compiled — it needs alloca (risky
# on the VM coro stacks) and is only used for rotated melee ship sprites, not
# menus/UI. rotateSurface/rotozoomSurfaceSize are stubbed (identity) in
# sdl_compat.c; revisit when melee lands.
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
  "$ROOT/src/main_cron.c"
  "$SDK/lib/cron_sys.c"
  "$SDK/lib/miniz.c"
  "$RT/picolibc.bc"
)

echo "[build] $(( ${#UQM_SRCS[@]} + ${#PORT[@]} )) translation units -> $OUT"

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
  "${UQM_SRCS[@]}" \
  "${SHIP_SRCS[@]}" \
  "${IP_SRCS[@]}" \
  "${GEN_SRCS[@]}" \
  "${SURF_SRCS[@]}" \
  "${UIO_SRCS[@]}" \
  "${RES_SRCS[@]}" \
  "${STR_SRCS[@]}" \
  "${GFX_SRCS[@]}" \
  "${GFXSDL_SRCS[@]}" \
  "${PORT[@]}" \
  --rom="$ROOT/content/uqm-0.8.0-content.uqm" \
  --heap-reserve=8M \
  --stack-reserve=512K \
  --title="UQM spike" \
  --author="Cronomantic (Cronopio port)" \
  --controls="N/A (threading spike — no UI yet)" \
  -o "$OUT"
