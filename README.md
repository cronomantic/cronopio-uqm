# cronopio-uqm

Port of [The Ur-Quan Masters](https://sc2.sourceforge.net/) (Star Control II)
to Cronopio / CronoVM.

**Status**: threading spike. The dominant porting risk is UQM's reliance on
real preemptive threads (`libs/threads/` has SDL + pthread backends, no
`BUILD_THREADED=false`). The spike adds a third backend driven by a
scheduler tick called from the cart's `frame()` and tries to run
`Starcon2Main` + the DCQ producer/consumer + the audio mixer through one
frame-fn without deadlock. If the spike succeeds in 1-2 weeks, the rest is
DOOM/Quake-style grind. If not, abort cheaply.

See the scouting report (memory: `uqm-port-scout`) and
[intgr/uqm-wasm](https://github.com/intgr/uqm-wasm) (Emscripten precedent
for the same retrofit).

## Layout

- `third_party/Cronopio/` — Cronopio platform (SDK + host + CronoVM), submodule
- `third_party/uqm/` — UQM upstream (cronomantic fork of SourceForge tree), submodule
- `src/` — cart-side platform seam (graphics, sound, input, sys, threads backend)
- `compat/` — shim headers (SDL stubs, config.h, etc.)
- `tools/` — cart build/packaging tools
- `content/` — UQM `.uqm` content packs (gitignored; base = 11.5 MB)
