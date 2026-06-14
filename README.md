# EoSD-PS2

A native PlayStation 2 port of Touhou 6 (The Embodiment of Scarlet Devil),
based on the GensokyoClub decompilation and its SDL2 "portable" fork.

The Direct3D/OpenGL rendering path has been replaced with a gsKit backend that
drives the GS directly. SDL2 (via the ps2dev port) is still used for input,
timing, WAV decoding and TTF text staging, none of which touch the GS.

### What changed from the portable fork

- New renderer `src/graphics/GsKitGfx.cpp` implementing `GfxInterface` on gsKit.
  Vertices are transformed on the EE and submitted as gouraud / textured GS
  triangles; VRAM residency is handled by gsKit's TexManager.
- All non-PS2 backends were removed (WebGL, fixed-function GL, software
  rasterizer) along with the desktop MIDI backends (only the MIDI stub remains)
  and the standalone config tool.
- A single memory module `src/MemAlloc.cpp`: the `MemAlloc` namespace is the
  global allocator (a `malloc`/`free` wrapper for process-lifetime data), and
  `MemArena` is a bump allocator for level-scoped memory, backed by `MemAlloc`
  and spilling to it on overflow. Two arenas exist: a per-stage `g_SceneArena`
  (reset on each `Stage::AddedCallback`) and a per-frame `g_FrameArena`.
- A minimalist start screen (`src/StartScreen.cpp`, gsKit ROM font) shown at
  boot. It asks where to keep save data: Memory Card 1, Memory Card 2, or no
  save/load at all.
- A global `FileManager` (`src/FileManager.cpp`) for all writable user data
  (config, high scores). The start screen picks its backend: Memory Card 1
  writes to `mc0:/TH06`, Memory Card 2 to `mc1:/TH06`, and the "no save/load"
  option keeps everything in RAM only and never persists it. It exposes
  `Exists` / `Read` / `Write` / `Delete`. Read-only game assets still come from
  the PBG3 archives via `FileSystem`.
- Debug output is console-only (no log file): lines are tagged `[TH06]` and
  `[TH06/Debug]` for the emulator/EE console.
- A plain `Makefile` (no premake) that builds with the ps2dev EE toolchain.

### Building

Requires the [ps2dev](https://github.com/ps2dev/ps2dev) toolchain with gsKit and
the SDL2 ports installed (the `PS2DEV` env var must point at the install, default
`/usr/local/ps2dev`).

```
make            # produces build/th06.elf
make iso        # produces build/th06.iso (bootable disc image)
make clean
```

`make iso` bundles the ELF, a generated `SYSTEM.CNF` and `audsrv.irx` into
`build/th06.iso`. Drop the original game files (the `.DAT` archives, `bgm/`, ...)
into the gitignored `basegame/` directory and they are copied onto the disc as
well, so a full playable image can be produced without committing copyrighted
data.

### Running

`build/th06.elf` can be launched in PCSX2 or on real hardware. The game data
files (the Japanese-named `.DAT` archives) must be reachable through the path the
host loader provides (e.g. `mass:/`, `host:/`, or `cdrom:/`); adjust the paths in
`FileSystem.cpp` for your loader if needed.

### Status / known gaps

- The build is complete and links cleanly; runtime has not yet been validated on
  hardware or in an emulator.
- GS fog (stage backgrounds) and per-pixel depth-mask are not wired up yet.
- The texture color-op is fixed to modulate, which covers the common path.

# Decomp Credits

We would like to extend our thanks to the following individuals for their
invaluable contributions:

- @EstexNT for porting the [`var_order` pragma](scripts/pragma_var_order.cpp) to
  MSVC7.
