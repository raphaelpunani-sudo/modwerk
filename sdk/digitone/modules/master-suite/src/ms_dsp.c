/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ms_dsp.h"
#include "ms_tables.h"

#define ONE30 (1LL << 30)

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* a (Q24) * x (Q30) -> Q30 */
static int64_t m24_30(int64_t a, int64_t x) { return (a * x) >> 24; }

/* Unsigned 64-bit division by shift and subtract: the ColdFire link may not
 * provide libgcc's __udivdi3. Only runs when a control changes. */
static uint64_t udiv(uint64_t n, uint64_t d, uint64_t *rem)
{
    uint64_t q = 0, r = 0;
    int i;
    for (i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= 1ULL << i; }
    }
    *rem = r;
    return q;
}

/* num/den with `bits` fraction bits (<= 28), long division in 7-bit
 * steps (no 128-bit, no float). den > 0. */
static int32_t div_q(int64_t num, int64_t den, int bits)
{
    int neg = num < 0;
    uint64_t n = neg ? (uint64_t)(-num) : (uint64_t)num, r, f = 0, q;
    int left = bits;
    q = udiv(n, (uint64_t)den, &r);
    while (left > 0) {
        int k = left < 7 ? left : 7;
        uint64_t d, rr = r << k;
        d = udiv(rr, (uint64_t)den, &r);
        f = (f << k) | d;
        left -= k;
    }
    q = (q << bits) | f;
    q += (r * 2 >= (uint64_t)den);                      /* round */
    if (q > 0x7FFFFFFFULL) q = 0x7FFFFFFFULL;
    return (int32_t)(neg ? -(int64_t)q : (int64_t)q);
}

static void set(struct ms_biquad *o, int64_t b0, int64_t b1, int64_t b2,
                int64_t a0, int64_t a1, int64_t a2)
{
    o->b0 = div_q(b0, a0, 27); o->b1 = div_q(b1, a0, 27); o->b2 = div_q(b2, a0, 27);
    o->a1 = div_q(a1, a0, 28); o->a2 = div_q(a2, a0, 28);
}

void ms_coef_filter(struct ms_biquad *o, int type, int cut, int res)
{
    int64_t omc, s, al;
    cut = clampi(cut, 0, 127);
    res = clampi(res, 0, 127);
    omc = ms_omc_filter[cut];
    s = ms_sin_filter[cut];
    al = m24_30(ms_inv2q[res], s);                       /* sin / (2Q) */
    {
        int64_t a0 = ONE30 + al, a1 = -2 * (ONE30 - omc), a2 = ONE30 - al;
        if (type == MS_FT_LP)
            set(o, omc / 2, omc, omc / 2, a0, a1, a2);
        else if (type == MS_FT_HP)
            set(o, (2 * ONE30 - omc) / 2, -(2 * ONE30 - omc), (2 * ONE30 - omc) / 2, a0, a1, a2);
        else
            set(o, al, 0, -al, a0, a1, a2);
    }
}

/* band 0 low shelf, 1 low-mid bell, 2 high-mid bell, 3 high shelf */
void ms_coef_eq(struct ms_biquad *o, int band, int freq, int gain)
{
    const int32_t *omct, *sint;
    int64_t omc, s, A, sA, Am1, Ap1, P, M, al, t2;
    freq = clampi(freq, 0, 127);
    gain = clampi(gain, 0, 60);
    switch (band) {
    case 0: omct = ms_omc_eq_ls; sint = ms_sin_eq_ls; break;
    case 1: omct = ms_omc_eq_lm; sint = ms_sin_eq_lm; break;
    case 2: omct = ms_omc_eq_hm; sint = ms_sin_eq_hm; break;
    default: omct = ms_omc_eq_hs; sint = ms_sin_eq_hs; break;
    }
    omc = omct[freq]; s = sint[freq];
    A = ms_a[gain]; sA = ms_sqrt_a[gain];                 /* Q24 */
    if (band == 1 || band == 2) {                         /* bell, Q = 1 */
        int64_t Ai = ms_a[60 - gain];                     /* 1/A */
        int64_t aA, aiA, a1 = -2 * (ONE30 - omc);
        al = s / 2;
        aA = m24_30(A, al); aiA = m24_30(Ai, al);
        set(o, ONE30 + aA, a1, ONE30 - aA, ONE30 + aiA, a1, ONE30 - aiA);
        return;
    }
    Am1 = A - (1 << 24); Ap1 = A + (1 << 24);
    /* shelf slope 1: alpha = sin / sqrt(2) */
    al = (s * 759250125LL) >> 30;                         /* 1/sqrt2 in Q30 */
    t2 = 2 * m24_30(sA, al);                              /* 2 sqrt(A) alpha */
    P = 2 * ONE30 + ((Am1 * omc) >> 24);                  /* (A+1)-(A-1)cos */
    M = ((2 * A) << 6) - ((Am1 * omc) >> 24);             /* (A+1)+(A-1)cos */
    if (band == 0) {
        int64_t b1 = 2 * m24_30(A, ((Ap1 * omc) >> 24) - 2 * ONE30);
        int64_t a1 = -2 * (((2 * A) << 6) - ((Ap1 * omc) >> 24));
        set(o, m24_30(A, P + t2), b1, m24_30(A, P - t2), M + t2, a1, M - t2);
    } else {
        int64_t b1 = -2 * m24_30(A, ((2 * A) << 6) - ((Ap1 * omc) >> 24));
        int64_t a1 = 2 * (((Ap1 * omc) >> 24) - 2 * ONE30);
        set(o, m24_30(A, M + t2), b1, m24_30(A, M - t2), P + t2, a1, P - t2);
    }
}

static const struct ms_biquad unity = { 1 << 27, 0, 0, 0, 0 };

void ms_init(struct ms_dsp *d)
{
    int i;
    for (i = 0; i < (int)sizeof *d; i++) ((uint8_t *)d)[i] = 0;
    d->f_cut = 90; d->f_res = 0; d->f_type = MS_FT_LP; d->f_on = 0;
    d->eq_on = 1;
    for (i = 0; i < MS_EQ_BANDS; i++) d->eq_gain[i] = MS_GAIN_ZERO;
    d->eq_freq[0] = 40; d->eq_freq[1] = 64; d->eq_freq[2] = 64; d->eq_freq[3] = 64;
    d->filt.cur = d->filt.tgt = unity;
    for (i = 0; i < MS_EQ_BANDS; i++) d->eq[i].cur = d->eq[i].tgt = unity;
    ms_update(d);
}

void ms_update(struct ms_dsp *d)
{
    int i;
    d->filt.active = d->f_on;
    if (d->f_on) ms_coef_filter(&d->filt.tgt, d->f_type, d->f_cut, d->f_res);
    for (i = 0; i < MS_EQ_BANDS; i++) {
        int on = d->eq_on && d->eq_gain[i] != MS_GAIN_ZERO;
        if (on) {
            ms_coef_eq(&d->eq[i].tgt, i, d->eq_freq[i], d->eq_gain[i]);
            if (!d->eq[i].active) {              /* entering: start from the target */
                d->eq[i].cur = d->eq[i].tgt;
                d->eq[i].st[0] = d->eq[i].st[1] = (struct ms_bq_state){0, 0, 0, 0, 0};
            }
        }
        d->eq[i].active = (uint8_t)on;
    }
    if (d->f_on && d->filt.cur.b0 == unity.b0 && d->filt.cur.a1 == 0) {
        d->filt.cur = d->filt.tgt;               /* first use: no glide from unity */
        d->filt.st[0] = d->filt.st[1] = (struct ms_bq_state){0, 0, 0, 0, 0};
    }
}

/* Move one coefficient a quarter of the way to its target each call. */
static int32_t glide(int32_t c, int32_t t)
{
    int32_t d = t - c;
    if (d > -4 && d < 4) return t;
    return c + (d >> 2);
}
static void glide_bq(struct ms_biquad *c, const struct ms_biquad *t)
{
    c->b0 = glide(c->b0, t->b0); c->b1 = glide(c->b1, t->b1); c->b2 = glide(c->b2, t->b2);
    c->a1 = glide(c->a1, t->a1); c->a2 = glide(c->a2, t->a2);
}

static void run(struct ms_stage *s, int32_t *lr, int frames)
{
    int ch, n;
    const struct ms_biquad c = s->cur;
    for (ch = 0; ch < 2; ch++) {
        struct ms_bq_state st = s->st[ch];
        int32_t *p = lr + ch;
        for (n = 0; n < frames; n++, p += 2) {
            int32_t x = *p;
            /* b is Q27, a is Q28: the feedback sum is halved to match */
            int64_t acc = (int64_t)c.b0 * x + (int64_t)c.b1 * st.x1 + (int64_t)c.b2 * st.x2
                        - (((int64_t)c.a1 * st.y1 + (int64_t)c.a2 * st.y2) >> 1)
                        + st.e;                         /* first-order error feedback */
            int64_t y = (acc + (1 << 26)) >> 27;
            if (y > MS_FULL) { y = MS_FULL; st.e = 0; }
            else if (y < -MS_FULL) { y = -MS_FULL; st.e = 0; }
            else st.e = (int32_t)(acc - y * (1LL << 27));     /* the rounding left over, +-2^26 */
            st.x2 = st.x1; st.x1 = x; st.y2 = st.y1; st.y1 = (int32_t)y;
            *p = (int32_t)y;
        }
        s->st[ch] = st;
    }
}

void ms_process(struct ms_dsp *d, int32_t *lr, int frames)
{
    int i;
    for (i = 0; i < MS_EQ_BANDS; i++) {
        if (!d->eq[i].active) continue;
        glide_bq(&d->eq[i].cur, &d->eq[i].tgt);
        run(&d->eq[i], lr, frames);
    }
    if (d->filt.active) {
        glide_bq(&d->filt.cur, &d->filt.tgt);
        run(&d->filt, lr, frames);
    }
}
