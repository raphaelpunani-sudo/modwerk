# Master Suite

A Digitone mod by @raphaelpunani-sudo. Version 0.1.0-experimental. **Work in progress: it does nothing on a unit yet.**

The first stage of a mastering chain: a filter and a 4-band EQ on the master mix. The other stages planned (tape, glue compressor, stereo imaging, limiter) are not started.

## What exists
- `src/ms_dsp.c`: the filter and EQ, fixed point, no floating point or libm. The EQ runs first and the filter last. A flat EQ and an off filter are an exact bypass.
- `src/ms_ui.c`: which knob does what, the press-and-turn coarse step, and the text each value shows.
- `src/main.c`: the render hook. It is a no-op until the master mix address is filled in.
- `tests/host/run.sh`: the DSP and control tests, run on a computer.

## Controls (design)
| Page | Knob | Does |
| --- | --- | --- |
| FILTER | E | Cutoff, 20 Hz to 18 kHz |
| FILTER | F | Resonance 0-127 |
| FILTER | G | Type: HP, LP, BP |
| FILTER | H | On/off |
| EQ | A-D | Gain of low shelf, low-mid bell, high-mid bell, high shelf: -15 to +15 dB, 0.5 dB steps |
| EQ | E | Low shelf frequency 30-450 Hz |
| EQ | F | Low-mid bell frequency 200 Hz - 2.5 kHz |
| EQ | G | High-mid bell frequency 600 Hz - 7 kHz |
| EQ | H | High shelf frequency 1.5-16 kHz |

Press and turn a knob for a coarse step (8 steps on frequencies, cutoff and resonance; 2 dB on EQ gains). Whether "press" means pressing the encoder or holding FUNC on the unit is not decided; see TESTING.md.

## Not done
See [TESTING.md](TESTING.md), "Open items".
