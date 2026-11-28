# Night in the Woods (`0100921006A04000`)

The registered build combines the original Switch base NSP with update 1.0.2 (`v131072`). It uses Unity 5.6.4p1 IL2CPP with FMOD, Nintendo SDK 3.5.2 and NVN API 53.313. The host loads `main`, `subsdk0` (NintendoSDK_movie) and `sdk`.

## What it needs

- The NPDM declares no system resource, so the profile reports virtual address memory disabled. It sets a 4 GiB base reservation with a 3.25 GiB heap region. The Mac boot requested a 3.14 GiB heap.
- The SDK's older VammManager functions match the existing shared SDK rewrite rule. [The SDK's address-region node area](../../../docs/reference/memory.md#the-sdks-address-region-node-area).

## Shaders

The stripped `Shader` type hash `a70f7abc6586fb35b3a0641aa81e9375` uses the existing Unity 5.5 layout. All 134 `Shader` objects decode to their exact ends, and their compressed subprograms unpack successfully. The split produces 1,398 vertex, 1,325 pixel and 12 compute shaders. All 2,735 built for Mac and iOS.

The [prewarm list](pso_manifest.tsv) records 42 render pipelines from startup through the bus station. On the first Mac run without a seed, two boot frames spent 190 ms compiling nine pipelines and loading their shaders. The packaged Mac app prewarmed all 42 pipelines in 31.5 ms and reached the title without pipeline compilation on its long frames. The game uses NVN's default depth convention.

## Mac playtest

Choose **Start New Game** with A (J on the Mac keyboard). Press A to advance each line of the prologue until Mae reaches the Possum Springs bus station. The first station dialogue renders over the scene.

On an M4 Pro with macOS 27.2.1 and Xcode 27.2, the 1920×1080 station scene averaged 59.8 fps over 25 seconds while standing still. Frame time was 16.69 ms median and 17.60 ms at the 99th percentile. The host reported about 3.84 GB resident, 3.34 GB of Metal allocations and zero audio underruns during the check.

## Found on this game

- [Unwind header with no search table](../../../docs/reference/rewriter.md#unwind-header-with-no-search-table).
- [Unity Shader layouts written out for players without bundles](../../../docs/reference/shaders.md#unity-shader-layouts-written-out-for-players-without-bundles).

## Open

- Later areas, save and reload, the standalone base version and iOS device gameplay are untested.
