/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Master Suite DSP: a filter and a 4-band EQ, fixed point, no libm, no FPU.
 *
 * Assumptions that are NOT verified on a unit (see TESTING.md):
 *  - 48 kHz (tables/gen_tables.py FS),
 *  - samples are signed 32-bit integers, stereo interleaved (MS_SAMPLE_*),
 *  - MS_FULL is the largest magnitude a sample may have.
 */
#ifndef MS_DSP_H
#define MS_DSP_H
#include <stdint.h>

#ifndef MS_FULL
#define MS_FULL 0x7FFFFFFF
#endif

#define MS_EQ_BANDS 4
#define MS_GAIN_ZERO 30            /* gain index of 0 dB (0.5 dB steps from -15) */

enum { MS_FT_HP, MS_FT_LP, MS_FT_BP };

struct ms_biquad {
    int32_t b0, b1, b2;             /* Q27: a high shelf at +15 dB needs |b1| > 8 */
    int32_t a1, a2;                 /* Q28 */
};
struct ms_bq_state { int32_t x1, x2, y1, y2, e; };

struct ms_stage {
    struct ms_biquad cur, tgt;
    struct ms_bq_state st[2];
    uint8_t active;                 /* 0: exact bypass */
};

struct ms_dsp {
    /* controls (the control model in ms_ui.c writes these) */
    uint8_t f_on, f_type, f_cut, f_res;            /* filter */
    uint8_t eq_on;
    uint8_t eq_gain[MS_EQ_BANDS];                  /* 0..60 */
    uint8_t eq_freq[MS_EQ_BANDS];                  /* 0..127 */
    /* engine */
    struct ms_stage filt;
    struct ms_stage eq[MS_EQ_BANDS];
};

void ms_init(struct ms_dsp *d);
/* Recompute targets from the controls; cheap, call after any control change. */
void ms_update(struct ms_dsp *d);
/* Process `frames` stereo frames, interleaved L,R, in place. EQ first, filter last. */
void ms_process(struct ms_dsp *d, int32_t *lr, int frames);

/* Exposed for the host tests */
void ms_coef_filter(struct ms_biquad *o, int type, int cut, int res);
void ms_coef_eq(struct ms_biquad *o, int band, int freq, int gain);
#endif
