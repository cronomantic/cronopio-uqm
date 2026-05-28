# UQM patches — staging area

Files under this tree are **proposed `[cronopio]` patches to UQM**. They will
eventually live in the `cronopio-port` branch of
[cronomantic/The-UrQuan-Masters](https://github.com/cronomantic/The-UrQuan-Masters),
mirrored under `third_party/uqm/` via the submodule pin.

Until then, this is a clean working area: nothing here is included by the
UQM build because the submodule's working tree is untouched. The build script
will copy / symlink these files into the submodule once we cut the
`cronopio-port` branch.

## Layout (mirrors the destination paths inside `sc2/`)

```
src/uqm-patches/
├── libs/threads/cron/
│   ├── cronthreads.h    → sc2/src/libs/threads/cron/cronthreads.h
│   └── cronthreads.c    → sc2/src/libs/threads/cron/cronthreads.c
├── libs/threads/
│   └── thrcommon.h.diff (NEW: adds THREADLIB_CRON case)
└── sdk/include/         (proposed Cronopio SDK additions, not UQM-specific)
    └── coro.h           → third_party/Cronopio/sdk/include/coro.h
```

## Prerequisites for these to compile

- CronoVM opcodes `CORO_INIT` (0x3C) + `CORO_SWAP` (0x3D) — see memory
  `cronovm-coro-design`.
- Cronopio SDK header `coro.h` + libc trampoline `cvm_coro.c` — drafted here
  under `sdk/include/coro.h`.
- `THREADLIB_CRON` case in `thrcommon.h` — drafted here as a patch.
