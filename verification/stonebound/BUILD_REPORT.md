# Stonebound build verification

Device: WeAct CH32X035F8U6 / WF86. Application begins at 0x3000.

| Profile | Flash bytes | Static RAM bytes |
|---|---:|---:|
| stonebound | 20476 | 12800 |
| stonebound-debug | 30048 | 13192 |

Both profiles fit 50,432 bytes of flash and 18,416 bytes of static RAM, with two flash save pages and a 2,048-byte stack reserved. The packager validates CRCs, target IDs and rejection by the Rev0 target. Physical hardware was not flashed or measured.

## Gameplay and integration checks

The successful [GitHub Actions run](https://github.com/veluv01/CHGame/actions/runs/38251669229) validated firmware source commit `12aa76a26bf850400577bc1fb91aac73dbcd6597` against CHGame base `c008a001a51f1eaa5087634193a04bbd4f0657c0`.

- 949,325 native checks under AddressSanitizer and UndefinedBehaviorSanitizer.
- 300 seeds across all six floors: 1,800 connected-floor fixtures.
- 53,566 randomized attempted actions, with state invariants and save roundtrips.
- 24 actual-sketch simulator checks, including story, menus, saves, end-screen fixtures and five-second START quit.
- Game state: 168 bytes; floor working data: 734 bytes.

Simulation does not measure physical CPU frame rate, flash persistence or electrical display/SD behaviour. No hardware was flashed. The packaged source adds download links and simulator captures after the validated firmware build; gameplay code is unchanged.
