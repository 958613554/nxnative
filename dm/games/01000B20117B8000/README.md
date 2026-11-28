# Re:ZERO -Starting Life in Another World- The Prophecy of the Throne (`01000B20117B8000`)

Base game 1.0.0 (`v0`, no update), SDK 9.5.0. Sibling title ID of [0100D800117F4000](../0100D800117F4000/README.md) (1.0.2), sharing the same `subsdk2` and `sdk` builds, memory layout, and **The Forge** renderer (`ReZERO-TPT`, Vulkan API 1.1). `main` imports `vkGetInstanceProcAddr` and loads every other Vulkan command through it; the host implements Vulkan on Metal (`dm/gfx/vk/hle_vk.m`). Modules `main`, `subsdk2` (NintendoSDK_movie) and `sdk`; `subsdk0` (NintendoSDK_Vulkan) and `subsdk1` (NintendoSDK_OpenGL) are not loaded.

Build IDs: `main` `1ef665d78f3de35c86c26f7b91d92784`, `subsdk2` `7a5e537af42c5847f02410d81096ff0e`, `sdk` `f93ce78898c2583ae08d87d3f2c2e4d6`.

## What it needs

- [The registered build and settings](game.tsv) use the 4 GiB layout (`mem_reserve 0x100000000`, `mem_alias 0x10000000`, `mem_stack 0x10000000`, `app_pool 0xCD500000`, `sys_resource 0x0`) and `modules main subsdk2 sdk` with no executable patches and no NVN shader tables.
- All Vulkan requirements found during bring-up — loader-only Vulkan detection and DXC SPIR-V entry points ([SPIR-V compiled by DXC](../../../docs/reference/rendering.md#spir-v-compiled-by-dxc), [Commands reached through the Vulkan loader](../../../docs/reference/rendering.md#commands-reached-through-the-vulkan-loader)), ordered replay and query copies ([Vulkan query copies consumed by the GPU](../../../docs/reference/rendering.md#vulkan-query-copies-consumed-by-the-gpu)), compute pipelines and Tier 2 argument buffers ([Vulkan compute dispatch](../../../docs/reference/rendering.md#vulkan-compute-dispatch), [Vulkan descriptor arrays over Metal's direct limits](../../../docs/reference/rendering.md#vulkan-descriptor-arrays-over-metals-direct-limits)), `LOAD_OP_DONT_CARE` attachment loading ([Attachments opened with `LOAD_OP_DONT_CARE`](../../../docs/reference/rendering.md#attachments-opened-with-load_op_dont_care)), and lowering `shadow-render.gs` viewport selection ([Vulkan geometry shaders that select a viewport](../../../docs/reference/rendering.md#vulkan-geometry-shaders-that-select-a-viewport)) — are now on `main`.

## Shaders

- 41 SPIR-V files in `fx/spirv/` compiled by DXC from HLSL (33 entry points not named `main`), translated offline by `dm/gfx/tools/vk_shaders.py`:
  - 9 fragment shaders with bindless descriptor arrays (`textures[512]`, `samplers[64]`, `genericSamplers[64]`);
  - 7 compute shaders (`skinning-p`, `-pn`, `-pnt`, `-pnts`, `ClusteredLight`, `SMAA-EdgeDetection`, and one more);
  - 1 geometry shader (`shadow-render.gs`, lowered into a vertex variant on `main`);
  - 3 vertex shaders reading a storage-buffer array `positionAttrIn[2]`;
  - 21 plain shaders (post effects, bloom, depth of field, motion blur, SMAA, copy, YUV).

## Mac testing

Brought up on Apple M4 Pro (macOS 27.0) on the pre-merge Vulkan stack whose fixes are now on `main`:

- 60 s boot with no input reached frame 3,300 (title menu up from frame 1,500), 0 `UNIMPL`, 0 crashes, 0 audio underruns; 30 draws, 2 dispatches, 13 passes, 1 submit, 0 skipped draws per frame at the title.
- Longer runs reached the prologue and first 2D village story scenes (25–27 draws, 4 dispatches, 0 skipped) and a 3D East hallway scene with lights, HUD, and Spine character art (52 draws, 18 dispatches, 13 passes, 0 skipped).

## Open

- Retesting `01000B20117B8000` directly on current `main` (including the landed `shadow-render.gs` viewport lowering), saves, sound, controller/touch input, the three WebM movies through `subsdk2`, extended play, and iPhone/iPad validation.
