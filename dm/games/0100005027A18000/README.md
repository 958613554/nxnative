# Graveyard Keeper 2 (`0100005027A18000`)

Switch update 1.0.5 (`v327680`), Nintendo SDK 21.4.4, Unity 6000.3.9f1 IL2CPP, NVN, FMOD. The profile loads `main`, `subsdk0` (NintendoSDK_movie) and `sdk` through the shared rewriter and host.

## Profile

- The NPDM requests 16 MiB of system resource. The game uses virtual address regions for its allocations. The startup check reached 2.69 GiB into the profile's 4.375 GiB alias region. The opening Mac playtest used that same region size.
- The game sets NVN's depth mode to near = 0, so the profile sets `depth_mode` to `zero` for the offline pipeline bases.
- The shader split found 14,958 entries in Unity serialized files and bundles under `Data/` and in `main`. All compiled for Mac and iOS. The prewarm list contains 88 render pipelines from boot, the title, and the start of the Inquisition Dungeon.
- DLC index 1 is a licence-only package. With the package present, the title screen showed the **Bonus Content Unlocked!** dialog for the costume.

## Mac playtest

Apple M4 Pro, macOS 27.2.1, Xcode 27.2 (27B52), 1920 × 1080 guest output. Build with `scripts/build_ipa_from_nsp.py --platform mac` and run the command it prints. Press A to dismiss the bonus content dialog if it appears, choose **Play**, then press A through the opening dialogue to reach the playable Inquisition Dungeon. With the character standing still at the game's default frame pacing, frames 7801–8100 had a median frame time of 33.35 ms.

## Open

Later gameplay, save reload, and iPhone and iPad playtests remain open.
