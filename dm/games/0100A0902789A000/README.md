# SpongeBob SquarePants: Titans of the Tide (`0100A0902789A000`)

1.4.0 from a merged `(1G+1U)` cartridge (`v0`), SDK 20.5.6. Unreal Engine 5.x (IoStore archives) on NVN, modules `main`, `subsdk0` and `sdk`. 19,563 shaders, 2,128 of them compute. Played on an iPhone 16 Pro: the menu, the first level, audio and a save, at 30 fps (frame time p50 33.4 ms), 30 minutes without a crash.

## What it needs

- **Nothing in `patches.py`.** The engine's `FMallocBinned3` is found by the rewriter's engine-wide search (`dm/tools/ue_malloc.py`): the UTF-16 `Binned3` name, its getter, the vtable slot and the constructor call are located in `main`, the call is cut and the host allocator takes its place.
- **Engine address region sizing.** During bring-up on iOS, an explicit `-SwitchVirtualAddressSpaceSize=4064` was needed so the page allocator did not run out of address space (at 2048 MB it aborted after about 13 minutes at `main+0x11cc`, and at 4096 MB it silently skipped creating the region and dereferenced null at `main+0x14b922c`). On current `main`, the rewriter's engine-wide gate (`dm/tools/ue_region.py`) sizes the engine region automatically from the available alias space, so `game.tsv` sets no `game_args`.
- **The memory partition**: `mem_reserve 0x200000000` (8 GB), `mem_alias 0x180000000` (6 GB), `mem_stack 0x10000000`, `app_pool 0xCD500000`, `sys_resource 0x1400000` (20 MB), and `stdalloc_cap 0x80000000` (2 GB) so the engine's region and the SDK's own `StandardAllocator` region both fit in the alias region. The provisional guess (66 GB reserve) cannot be mapped on iOS (`FATAL reserve guest AS`).

| key | value |
|---|---|
| `mem_reserve` | `0x200000000` (8 GB) |
| `mem_alias` | `0x180000000` (6 GB) |
| `mem_stack` | `0x10000000` |
| `app_pool` | `0xCD500000` |
| `sys_resource` | `0x1400000` (20 MB) |
| `stdalloc_cap` | `0x80000000` (2 GB) |

## Frames

30 fps: `frame time p50 33.41 ms / p95 34.83 ms` over 300-frame windows, which is the game's own pacing. 1,173 draws a frame in the first level; the GPU is the limiter there (long frames 43–50 ms when a new area streams in).

## Tables

Per-build shader tables (`shader_map.tsv`, `shader_facts.tsv`, `shader_pairs.tsv`) are generated at pack time from the built version's `RomFS.img` and `main`: `shaders: 19563 (compute 2128, pixel 9033, vertex 8402); sources game 17679, global 1884; variation pairs 16804`.

## Open

- Re-verifying the current pack (with `ue_region.py` and build-time shader splitting), Mac gameplay, and recording `pso_manifest.tsv`.
