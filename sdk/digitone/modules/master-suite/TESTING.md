# Master Suite testing

Version 0.1.0-experimental. Evidence tier: none. Nothing has been built for the ColdFire, flashed or run in an emulator.

## What was run (on a computer, `tests/host/run.sh`, gcc with UBSan)
Float is used only as the reference. Assumed 48 kHz and signed 32-bit samples.

| Check | Result |
| --- | --- |
| Filter coefficients vs RBJ reference, 3 types x resonance x cutoff, 14 probe frequencies | worst error 0.013 dB |
| EQ coefficients vs reference, 4 bands x 31 gains x 26 frequencies | worst error 0.0035 dB |
| Filter, measured through the real process loop, 27 settings | worst error 0.005 dB (settings whose response is below -60 dB or above +18 dB are skipped) |
| EQ, measured through the real process loop, 48 settings | worst error 0.001 dB |
| Default settings are a bit-exact passthrough | pass |
| Idle after 1 s of loud noise then 5 s of silence, 30 extreme settings with the EQ at +15 dB | worst tail 3 (32-bit LSB), below 1 LSB of 24-bit |
| Cutoff sweep at maximum resonance, 4 s, 220 Hz tone | no overflow; largest sample step 3.1% of full scale. This is a blow-up check, not proof of no clicks. |
| DC with a 1 kHz tone, EQ shelves at +15 dB, 20 Hz high-pass | -26,369 of 2^31 (about -98 dBFS) |
| Control ranges, clamps, coarse step and value text | pass |

Total 576,078 checks, 0 failed. Two real bugs were found and fixed on the way: a wrong 1/sqrt(2) constant in the shelves, and a high shelf at +15 dB whose b1 does not fit Q28 (coefficients are now Q27 feed-forward, Q28 feedback). Rounding left a constant offset of about -83 dBFS after the input stopped; first-order error feedback removed it.

## Resources (estimates from a host build, not measured on a unit)
- Shared mod memory: about 14.5 KB of 128 KiB (tables 8.7 KB, code about 5 KB on x86, state 436 B). The m68k size will differ.
- Fast SRAM: 0 of 2,304 bytes. The per-sample loop is not placed in fast SRAM yet.
- CPU: not measured. Per sample the cost is 5 multiplies per active biquad per channel, with at most 5 active biquads (4 EQ bands and the filter). Not compared with Spring Reverb.

## Open items (needed before it can run)
1. **Master mix address and format (OS 1.44).** Where the render leaves the stereo mix, the frames per block and the sample format. This comes from the owner's own firmware, which must not be committed. Fill `MS_MIX_ADDR` and `MS_MIX_FRAMES` in `src/main.c`.
2. **Which render hook sees the final mix.** `ev_render_out` is used; whether the main outs, headphones and USB all carry it is unknown.
3. **Sample rate.** 48 kHz is assumed. If wrong, regenerate with `python3 -B tools/gen_tables.py > src/ms_tables.h`.
4. **The page.** Registering one mod page after the External Mixer pages, drawing it, reading key and encoder events, and the page-order question (second page when dual parameter is off).
5. **Press and turn.** Decide whether it is an encoder press or FUNC, and wire it to the `fast` argument of `ms_turn`.
6. **Build.** Node 24, `npm ci`, `npm run module:doctor -- master-suite`, `npm run pr:ready`. None was run: the registry was unreachable from where this was written. The ColdFire link may lack libgcc's 64-bit multiply.
7. **Saving settings with the project** (`core_projdata`) is not written.
8. **Stock flows** (mixer pages, saving, MIDI, USB) were not compared with and without the module.
9. **Hardware and emulator tests, persistence, reboot, CPU headroom.** Not tested.
