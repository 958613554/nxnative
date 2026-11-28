# Cyberpunk 2077 (`0400DDB020648000`)

A Switch 2 game: update v262144, SDK 20.5.3 built for the Switch 2 (`Ounce`), NVN2. CD PROJEKT RED's REDengine 4 with Wwise. Modules `main` and `sdk`; `subsdk0` and `subsdk1` (C / libc) are not loaded. It plays on the Mac: from the title screen through New Game, Phantom Liberty's Dogtown opening, and the Nomad garage save in Quality mode with 1920×1080 MetalFX temporal upscaling. Only v262144 has been run; the base game and iPhone/iPad builds have not been validated.

## What it needs

| Setting | Where | Why |
|---|---|---|
| `mem_reserve 0x140000000`, `mem_alias 0x118000000`, `mem_stack 0x10000000`, `app_pool 0x240000000` | [`game.tsv`](game.tsv) | Switch 2 9 GB pool; `main` maps virtual address memory itself (Mac uses the 128 GB Switch 2 virtual reservation; guest alias regions reach +38.66 GB) |
| `sys_resource 0x4000000` | `game.tsv` | the NPDM declares 64 MB of system resource, enabling virtual address memory |
| `modules main sdk` | `game.tsv` | `main` takes nothing from `subsdk0` or `subsdk1` that the host does not supply |
| `romfs_marker archive/emporium/ep1/ep1_3_nightcity_gi.archive`, `zip_store .archive:.bin:.cache:.nro:.redscripts:.metallib` | `game.tsv` | RomFS is 59.83 GB of REDengine archives and caches |
| `shader_groups /archive/=archive /engine/=engine /r6/=r6 /nro/=nro /.nrr/=nrr` | `game.tsv` | shaders live in [REDengine 4 shader caches](../../../docs/reference/shaders.md#redengine-4-shader-caches) |
| `depth_mode zero` | `game.tsv` | the game calls `nvnDeviceSetDepthMode` with near = 0 |
| `publish_shader_after_initialization` | [`patches.py`](patches.py) | build-gated (`afd670b6ba27b9e68d268e601bf91acd`, v262144): REDengine reserves a shader-cache slot and sets its reference count to 1 at `main+0x84dd30` before filling the record, so another worker can read a null control pointer (`mtl_prog_prefetch` abort); the patch keeps the count nonpositive until the three load paths finish and publishes it with `stlr` |

[`pso_manifest.tsv`](pso_manifest.tsv) seeds pipeline prewarm from Mac captures covering the opening, Dogtown, and Nomad gameplay.

## Shaders

6,754 distinct shaders in `shader_map.tsv` (3,608 vertex, 2,803 pixel, 343 compute) extracted from 18,567 material and 530 static records in the REDengine 4 caches. On chain `7c0c514fde33` all 6,754 build cleanly for the Mac.

## Found on this game

- [REDengine 4 shader caches](../../../docs/reference/shaders.md#redengine-4-shader-caches) — shader split extraction of material and static shader records.
- [Early fragment tests from the control section](../../../docs/reference/shaders.md#early-fragment-tests-from-the-control-section) and [Bindless texture-pool read marks disabled Apple Silicon TBDR hidden surface removal](../../../docs/reference/shaders.md#bindless-texture-pool-read-marks-disabled-apple-silicon-tbdr-hidden-surface-removal).
- [Raw global memory lookups that start from their last entry](../../../docs/reference/shaders.md#raw-global-memory-lookups-that-start-from-their-last-entry) — shared register-cached range/delta, same-base coalescing, 4 GiB `gmem_map` range splitting, single-pool 2 MiB hints, and 2-entry cache for multi-site shaders.
- [Wrapped 1D fetches through a bindless pool lose buffer textures](../../../docs/reference/shaders.md#wrapped-1d-fetches-through-a-bindless-pool-lose-buffer-textures) — fixed the 16×16 top-left black tile before temporal upscaling (`c0ae77649d1fdff0`).
- [A dynamic size query reads the wrong texture pool](../../../docs/reference/shaders.md#a-dynamic-size-query-reads-the-wrong-texture-pool) — 3D volume size queries in `2f537e583274ea15`.
- [Active render targets in the bindless uncompressed-alias list collapsed passes to one draw](../../../docs/reference/rendering.md#active-render-targets-in-the-bindless-uncompressed-alias-list-collapsed-passes-to-one-draw) — eliminated 1,200-pass collapses and bindless alias copy storms.
- Pool depth snapshots, bindless image pool admission and rebuilds, consecutive clear batching, buffer-copy upload dependencies, and dedicated MetalFX temporal queue in [rendering](../../../docs/reference/rendering.md).
- Sparse virtual pool deferred buffer materialization, page-granularity writeback comparison, ordered shadow-range lookups, and shadow-backing reuse in [NVN objects](../../../docs/reference/nvn-objects.md).

## Driving it on the Mac

`python3 scripts/build_ipa_from_nsp.py <folder> --platform mac` builds the shaders, host, and Mac app. Quality mode with 1920×1080 temporal MetalFX runs cleanly on Apple silicon Macs.

**Measured on Apple M6 (16 GB, macOS 27.2.1, Xcode 27.2, Quality + 1920×1080 temporal MetalFX, standing Nomad save, chain `7c0c514fde33`):**
- Frame time p50 `29.28 ms` (`34.2 fps`), GPU time p50 `28.87 ms` (min `25.82 ms`, `34.6–38.7 fps`), NVN submit+present CPU p50 `11.75 ms`, 0 new `[RED]` lines.

## Open

- **Sustained 60 fps (`~16.7 ms`):** standing Nomad currently sits at `~28.9 ms` GPU p50 (`34.6 fps`). Active targets include G-buffer `fb_pool_depth` snapshots / pass splits (`6–7` full 1920×1080 D32S8 copies per frame), remaining compute/deferred shader hotspots, and CPU submit time.
- **First 3D frame after loading:** streaming still hits ~21,000 dirty-page faults and ~50–60 ms of GPU work on the first scene frame.
- **iPhone/iPad:** not tested; guest virtual alias regions reach +38.66 GB (past the 4.375 GB iPhone alias layout in `game.tsv`).
- **Base build (`v0`):** untested; `patches.py` is gated to `v262144` (`afd670b6ba27b9e68d268e601bf91acd`).
