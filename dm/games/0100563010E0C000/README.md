# WarioWare: Get It Together! (`0100563010E0C000`)

Switch base build 1.0.0 (`v0`), Nintendo SDK 10.4.1 and NintendoWare Bezel Engine on NVN. The modules are `main`, `subsdk0` (GLSLC), `subsdk1` (NintendoSDK_movie) and `sdk` in Horizon's load order. The rewriter's existing rules cover all four modules.

## What it needs

| Setting | Why |
|---|---|
| `sys_resource 0x0` | The NPDM declares no system resource, so the SDK uses its heap path. Boot requested a 3.14 GB heap in the profile's 3.25 GB heap region. |
| `depth_mode zero` | The game sets the near plane to zero and the upper-left window origin. Prebuilt pipelines use the same depth entry point as live rendering. |

The usual [Bezel Engine archive reader](../../../docs/reference/shaders.md#bnsh-in-bezel-engine-archives) finds 11,906 shader stages in the game's 420 `.bea` archives, movies and embedded code. The split finds 2,662 vertex, 9,202 pixel, 14 geometry and 28 compute stages. Shader tables are generated from the base NSP during packing.

## Mac checks

On an Apple M4 Pro with macOS 27.2.1, the game played its opening movie, showed the title and mode menu, created a save, and reached Wario's Intro Games in one-player Story at 1600 × 900. The title's median frame time was 16.68 ms (about 60 fps). The host opened real audio output and logged no underruns in the sampled callback windows.

One `[RED]` at the transition to the title reported an unbound vertex uniform slot 4 for `bd6f846d3a963f15` from `Graphics.nx.bea`. The translated shader checks the binding size on all its reads from that slot. The host supplies the zero bank and zero size, so those reads return zero. See [An unbound uniform slot reads the zero bank](../../../docs/reference/rendering.md#an-unbound-uniform-slot-reads-the-zero-bank).

In the Mac host window, J sends A, K sends B and Return sends +. Enable the Metal HUD in the launcher settings or use `DM_METAL_HUD=1` with the host.

## Packaging

Xcode 27.2 with `metalfe-32023.922.4` compiled all 11,906 stages for macOS and iOS. The Mac app and unsigned IPA ship with the RomFS and the [prewarm list](pso_manifest.tsv), which holds 184 render pipelines and one compute shader recorded on the Mac route above. The Mac app builds its pipeline bases on first launch.

## Open

- Completing Story microgames, multiplayer and running the unsigned IPA on an iPhone.
