# Graveyard Keeper 2 (`0100005027A18000`)

Switch update 1.0.5 (`v327680`), Nintendo SDK 21.4.4, Unity 6000.3.9f1 IL2CPP, NVN, FMOD. The profile loads `main`, `subsdk0` (NintendoSDK_movie) and `sdk` through the shared rewriter and host.

## Profile

- The NPDM requests 16 MiB of system resource. The game uses virtual address regions for its allocations. The startup check reached 2.69 GiB into the profile's 4.375 GiB alias region. The opening Mac playtest used that same region size.
- The game sets NVN's depth mode to near = 0, so the profile sets `depth_mode` to `zero` for the offline pipeline bases.
- The shader split found 14,958 entries in Unity serialized files and bundles under `Data/` and in `main`. All compiled for Mac and iOS. The prewarm list contains 88 render pipelines from boot, the title, and the start of the Inquisition Dungeon.
- DLC index 1 is a licence-only package. With the package present, the title screen showed the **Bonus Content Unlocked!** dialog for the costume.

## Mac playtest

Apple M4 Pro, macOS 27.2.1, Xcode 27.2 (27B52), 1920 × 1080 guest output. Build with `scripts/build_ipa_from_nsp.py --platform mac` and run the command it prints. Press A to dismiss the bonus content dialog if it appears, choose **Play**, then press A through the opening dialogue to reach the playable Inquisition Dungeon.

## Frame rate

The profile defaults to 60 fps. The player's **Frame rate** setting also offers 30 fps. Unity's [NVN vblank period](../../../docs/reference/frame-rate.md#unitys-nvn-device-waits-for-its-own-vblank-period) holds the stock game to 30 fps. On 1.0.5, the rewriter recognizes the vsync count at `main+0x7db3e18` and the wait duration at `main+0x7db3e24`. It finds the same fields at `main+0x7b40398` and `main+0x7b403a4` on 1.0.0 in an offline check.

A Mac sample caught the main thread blocked in the SDK call at `main+0xd598ec`. Display mode alone delivered 30.0 fps. A diagnostic run that changed only the duration stored by Unity, with display mode held at 60 Hz, reached 59.9 fps on the same route.

On the M4 Pro, both profile settings reached the playable dungeon by 78 seconds on the same scripted route from fresh saves. Standing still in the dungeon from 90 to 100 seconds, the 30 fps setting measured 30.0 fps and a 33.38 ms median frame time. The default measured 59.9 fps and 16.68 ms. Both samples had zero dropped frames. The host estimated about 0.60 W of CPU power at 30 fps and 0.98 W at 60 fps in the standing scene.

## Open

Later gameplay, save reload, and iPhone and iPad playtests remain open.
