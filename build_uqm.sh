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
  "$SDK/lib/cvm_libc.c"
  "$SDK/lib/miniz.c"
)

echo "[build] $(( ${#UQM_SRCS[@]} + ${#PORT[@]} )) translation units -> $OUT"

# -DCRONOPIO routes config.h to compat/config_cron.h and libs/log.h to
# compat/log_cron.h (both via our [cronopio] patches on the
# cronopio-port branch of the fork).
"$CC" \
  -DCRONOPIO \
  -D_POSIX_NAME_MAX=255 \
  -D_POSIX_PATH_MAX=256 \
  -DMINIZ_NO_DEFLATE_APIS -DMINIZ_NO_ARCHIVE_APIS \
  -DMINIZ_NO_STDIO -DMINIZ_NO_TIME \
  -DHAVE_ZIP \
  "${INCS[@]}" \
  "${UQM_SRCS[@]}" \
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
