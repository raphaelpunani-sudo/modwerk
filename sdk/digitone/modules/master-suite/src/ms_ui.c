/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ms_ui.h"
#include "ms_tables.h"

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int step(int delta, int fast, int coarse) { return delta * (fast ? coarse : 1); }

static char *put_uint(char *p, unsigned v)
{
    char t[10]; int n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *p++ = t[--n];
    return p;
}
/* 1234 -> "1.2k", 440 -> "440", 16000 -> "16k" */
static void put_hz(char *p, unsigned hz)
{
    if (hz < 1000) { p = put_uint(p, hz); *p = 0; return; }
    p = put_uint(p, hz / 1000);
    if (hz < 10000 && (hz / 100) % 10) { *p++ = '.'; *p++ = (char)('0' + (hz / 100) % 10); }
    *p++ = 'k'; *p = 0;
}
static const int32_t *hz_table(int page, int knob)
{
    if (page == MS_PAGE_FILTER) return ms_hz_filter;
    switch (knob) {
    case MS_KNOB_E: return ms_hz_eq_ls;
    case MS_KNOB_F: return ms_hz_eq_lm;
    case MS_KNOB_G: return ms_hz_eq_hm;
    default: return ms_hz_eq_hs;
    }
}

int ms_turn(struct ms_dsp *d, int page, int knob, int delta, int fast)
{
    if (page == MS_PAGE_FILTER) {
        switch (knob) {
        case MS_KNOB_E: d->f_cut = (uint8_t)clampi(d->f_cut + step(delta, fast, 8), 0, 127); break;
        case MS_KNOB_F: d->f_res = (uint8_t)clampi(d->f_res + step(delta, fast, 8), 0, 127); break;
        case MS_KNOB_G: d->f_type = (uint8_t)clampi(d->f_type + delta, MS_FT_HP, MS_FT_BP); break;
        case MS_KNOB_H: if (delta) d->f_on = delta > 0; break;
        default: return 0;
        }
    } else if (page == MS_PAGE_EQ) {
        if (knob <= MS_KNOB_D) d->eq_gain[knob] = (uint8_t)clampi(d->eq_gain[knob] + step(delta, fast, 4), 0, 60);
        else d->eq_freq[knob - MS_KNOB_E] = (uint8_t)clampi(d->eq_freq[knob - MS_KNOB_E] + step(delta, fast, 8), 0, 127);
    } else {
        return 0;
    }
    ms_update(d);
    return 1;
}

int ms_text(const struct ms_dsp *d, int page, int knob, char *out)
{
    if (page == MS_PAGE_FILTER) {
        switch (knob) {
        case MS_KNOB_E: put_hz(out, (unsigned)hz_table(page, knob)[d->f_cut]); return 1;
        case MS_KNOB_F: *put_uint(out, d->f_res) = 0; return 1;
        case MS_KNOB_G: {
            static const char *const n[] = { "HP", "LP", "BP" };
            const char *s = n[d->f_type > 2 ? 2 : d->f_type];
            out[0] = s[0]; out[1] = s[1]; out[2] = 0; return 1;
        }
        case MS_KNOB_H: {
            const char *s = d->f_on ? "ON" : "OFF";
            while ((*out++ = *s++)) ;
            return 1;
        }
        default: return 0;
        }
    }
    if (page == MS_PAGE_EQ) {
        if (knob <= MS_KNOB_D) {
            int half = d->eq_gain[knob] - MS_GAIN_ZERO;      /* in 0.5 dB */
            unsigned a = (unsigned)(half < 0 ? -half : half);
            char *p = out;
            if (half) *p++ = half < 0 ? '-' : '+';
            p = put_uint(p, a / 2);
            if (a & 1) { *p++ = '.'; *p++ = '5'; }
            *p = 0;
        } else {
            put_hz(out, (unsigned)hz_table(page, knob)[d->eq_freq[knob - MS_KNOB_E]]);
        }
        return 1;
    }
    return 0;
}

const char *ms_label(int page, int knob)
{
    static const char *const f[] = { 0, 0, 0, 0, "CUT", "RES", "TYPE", "ON" };
    static const char *const e[] = { "LS", "LM", "HM", "HS", "LSF", "LMF", "HMF", "HSF" };
    if (page == MS_PAGE_FILTER) return knob >= 0 && knob < 8 ? f[knob] : 0;
    if (page == MS_PAGE_EQ) return knob >= 0 && knob < 8 ? e[knob] : 0;
    return 0;
}
