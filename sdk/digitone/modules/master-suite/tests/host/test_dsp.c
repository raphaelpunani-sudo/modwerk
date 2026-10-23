/* Host tests for Master Suite's DSP. Float is used here only, as the reference. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/ms_dsp.h"
#include "../../src/ms_ui.h"
#include "../../src/ms_tables.h"

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static const double FS = 48000.0, TWO_PI = 6.283185307179586;
static double fr(double lo, double hi, int i) { return lo * pow(hi / lo, i / 127.0); }

/* ---- float reference (RBJ cookbook) ---- */
struct fb { double b0, b1, b2, a1, a2; };
static struct fb norm(double b0, double b1, double b2, double a0, double a1, double a2)
{ struct fb r = { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 }; return r; }
static double mag_db(struct fb c, double f)
{
    double w = TWO_PI * f / FS, cr = cos(w), si = sin(w), c2 = cos(2 * w), s2 = sin(2 * w);
    double nr = c.b0 + c.b1 * cr + c.b2 * c2, ni = -(c.b1 * si + c.b2 * s2);
    double dr = 1 + c.a1 * cr + c.a2 * c2, di = -(c.a1 * si + c.a2 * s2);
    return 10 * log10((nr * nr + ni * ni) / (dr * dr + di * di));
}
static struct fb ref_filter(int type, double f, double Q)
{
    double w = TWO_PI * f / FS, c = cos(w), al = sin(w) / (2 * Q);
    if (type == MS_FT_LP) return norm((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    if (type == MS_FT_HP) return norm((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    return norm(al, 0, -al, 1 + al, -2 * c, 1 - al);
}
static struct fb ref_eq(int band, double f, double db)
{
    double A = pow(10, db / 40), w = TWO_PI * f / FS, c = cos(w), s = sin(w);
    if (band == 1 || band == 2) {
        double al = s / 2;
        return norm(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    } else {
        double al = s / sqrt(2.0), t = 2 * sqrt(A) * al;
        if (band == 0)
            return norm(A * ((A + 1) - (A - 1) * c + t), 2 * A * ((A - 1) - (A + 1) * c),
                        A * ((A + 1) - (A - 1) * c - t), (A + 1) + (A - 1) * c + t,
                        -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - t);
        return norm(A * ((A + 1) + (A - 1) * c + t), -2 * A * ((A - 1) + (A + 1) * c),
                    A * ((A + 1) + (A - 1) * c - t), (A + 1) - (A - 1) * c + t,
                    2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t);
    }
}
static struct fb q28(const struct ms_biquad *b)
{
    struct fb r = { b->b0 / 134217728.0, b->b1 / 134217728.0, b->b2 / 134217728.0,
                    b->a1 / 268435456.0, b->a2 / 268435456.0 };
    return r;
}
static double Qof(int v) { return 0.5 + 11.5 * pow(v / 127.0, 2); }

/* ---- 1. coefficient accuracy against the reference, every table step ---- */
static void test_coefs(void)
{
    double worst_f = 0, worst_e = 0;
    int t, i, g, b;
    static const double probe[] = { 20, 31, 50, 80, 120, 200, 400, 800, 1500, 3000, 6000, 10000, 16000, 21000 };
    for (t = 0; t < 3; t++)
        for (i = 0; i < 128; i += 3) {
            int r;
            for (r = 0; r < 128; r += 21) {
                struct ms_biquad c; struct fb ref, got; unsigned k;
                ms_coef_filter(&c, t, i, r);
                got = q28(&c); ref = ref_filter(t, fr(20, 18000, i), Qof(r));
                for (k = 0; k < sizeof probe / sizeof *probe; k++) {
                    double e = fabs(mag_db(got, probe[k]) - mag_db(ref, probe[k]));
                    /* far into a stop band the response is below -100 dB: not audible, not judged */
                    if (mag_db(ref, probe[k]) < -90) continue;
                    if (e > worst_f) worst_f = e;
                }
            }
        }
    printf("filter coefficient worst error: %.4f dB\n", worst_f);
    CHECK(worst_f < 0.1, "filter coefficients differ from reference by %.3f dB", worst_f);
    static const double lo[] = { 30, 200, 600, 1500 }, hi[] = { 450, 2500, 7000, 16000 };
    for (b = 0; b < 4; b++)
        for (g = 0; g < 61; g += 2)
            for (i = 0; i < 128; i += 5) {
                struct ms_biquad c; struct fb ref, got; unsigned k;
                ms_coef_eq(&c, b, i, g);
                got = q28(&c); ref = ref_eq(b, fr(lo[b], hi[b], i), -15 + 0.5 * g);
                for (k = 0; k < sizeof probe / sizeof *probe; k++) {
                    double e = fabs(mag_db(got, probe[k]) - mag_db(ref, probe[k]));
                    if (e > worst_e) worst_e = e;
                }
            }
    printf("EQ coefficient worst error: %.4f dB\n", worst_e);
    CHECK(worst_e < 0.1, "EQ coefficients differ from reference by %.3f dB", worst_e);
}

/* ---- helpers: measured gain of the whole engine at one frequency ---- */
static double measure_db(struct ms_dsp *d, double f, double amp)
{
    int n, N = 48000 * 2, skip = 48000;
    static int32_t buf[2 * 48000 * 2];
    double in = 0, out = 0;
    for (n = 0; n < N; n++) { int32_t v = (int32_t)lrint(amp * sin(TWO_PI * f * n / FS)); buf[2 * n] = buf[2 * n + 1] = v; }
    for (n = 0; n < N; n += 64) ms_process(d, buf + 2 * n, 64);
    for (n = skip; n < N; n++) {
        double x = amp * sin(TWO_PI * f * n / FS), y = buf[2 * n];
        in += x * x; out += y * y;
    }
    return 10 * log10(out / in);
}

/* ---- 2. whole engine: measured vs reference, through the real process loop ---- */
static void test_end_to_end(void)
{
    static const double probe[] = { 40, 100, 300, 1000, 3000, 8000, 15000 };
    unsigned k; int t, band;
    double worst = 0;
    for (t = 0; t < 3; t++) {
        int cut[] = { 10, 64, 100 }, res[] = { 0, 60, 127 }, ci, ri;
        for (ci = 0; ci < 3; ci++) for (ri = 0; ri < 3; ri++) {
            struct ms_dsp d; ms_init(&d);
            d.f_on = 1; d.f_type = t; d.f_cut = cut[ci]; d.f_res = res[ri]; d.eq_on = 0;
            ms_update(&d);
            for (k = 0; k < sizeof probe / sizeof *probe; k++) {
                double want = mag_db(ref_filter(t, fr(20, 18000, cut[ci]), Qof(res[ri])), probe[k]);
                struct ms_dsp e = d; double got;
                if (want < -60 || want > 18) continue;      /* out of the measurable range at this level */
                got = measure_db(&e, probe[k], 1.0e8);
                if (fabs(got - want) > worst) worst = fabs(got - want);
            }
        }
    }
    printf("filter end-to-end worst error: %.3f dB\n", worst);
    CHECK(worst < 0.3, "filter measured differs from reference by %.3f dB", worst);
    worst = 0;
    static const double lo[] = { 30, 200, 600, 1500 }, hi[] = { 450, 2500, 7000, 16000 };
    for (band = 0; band < 4; band++) {
        int gains[] = { 0, 12, 48, 60 }, gi, fi[] = { 0, 64, 127 }, fj;
        for (gi = 0; gi < 4; gi++) for (fj = 0; fj < 3; fj++) {
            struct ms_dsp d; ms_init(&d);
            d.eq_gain[band] = gains[gi]; d.eq_freq[band] = fi[fj]; ms_update(&d);
            for (k = 0; k < sizeof probe / sizeof *probe; k++) {
                double want = mag_db(ref_eq(band, fr(lo[band], hi[band], fi[fj]), -15 + 0.5 * gains[gi]), probe[k]);
                struct ms_dsp e = d; double got = measure_db(&e, probe[k], 1.0e8);
                if (fabs(got - want) > worst) worst = fabs(got - want);
            }
        }
    }
    printf("EQ end-to-end worst error: %.3f dB\n", worst);
    CHECK(worst < 0.15, "EQ measured differs from reference by %.3f dB", worst);
}

/* ---- 3. flat EQ and off filter are bit-exact passthrough ---- */
static void test_bypass(void)
{
    struct ms_dsp d; static int32_t a[2 * 4096], b[2 * 4096]; int i;
    ms_init(&d); srand(7);
    for (i = 0; i < 2 * 4096; i++) a[i] = b[i] = (int32_t)(((uint32_t)rand() << 16) ^ (uint32_t)rand());
    ms_process(&d, b, 4096);
    CHECK(memcmp(a, b, sizeof a) == 0, "default settings are not a bit-exact passthrough");
}

/* ---- 4. stability at the extremes, and settling after the input stops ---- */
static void test_stability(void)
{
    int t, ci, ri, n;
    long worst_tail = 0;
    int cuts[] = { 0, 1, 64, 126, 127 };
    for (t = 0; t < 3; t++) for (ci = 0; ci < 5; ci++) for (ri = 0; ri < 2; ri++) {
        struct ms_dsp d; static int32_t buf[2 * 64]; long tail = 0;
        ms_init(&d); d.f_on = 1; d.f_type = t; d.f_cut = cuts[ci]; d.f_res = ri ? 127 : 0;
        d.eq_gain[0] = 60; d.eq_gain[1] = 0; d.eq_gain[2] = 60; d.eq_gain[3] = 0;
        d.eq_freq[0] = 0; d.eq_freq[2] = 127; ms_update(&d);
        srand(3);
        for (n = 0; n < 48000 / 64; n++) {             /* 1 s of loud noise */
            int i; for (i = 0; i < 128; i++) buf[i] = (int32_t)(((rand() & 0x7FFFFF) - 0x400000) * 256);
            ms_process(&d, buf, 64);
        }
        for (n = 0; n < 48000 * 6 / 64; n++) {         /* 6 s of silence */
            int i; memset(buf, 0, sizeof buf);
            ms_process(&d, buf, 64);
            if (n >= 48000 * 5 / 64) for (i = 0; i < 128; i++) if (labs(buf[i]) > tail) tail = labs(buf[i]);
        }
        if (tail > worst_tail) worst_tail = tail;
        CHECK(tail < 256, "type %d cut %d res %d did not settle: tail %ld LSB(32-bit)", t, cuts[ci], ri ? 127 : 0, tail);
    }
    printf("worst idle tail after 5 s of silence: %ld (32-bit LSB; 256 = 1 LSB of 24-bit)\n", worst_tail);
}

/* ---- 5. moving a control while playing: no blow-up, no big step ---- */
static void test_sweep(void)
{
    int t, n, i; double worst = 0;
    for (t = 0; t < 3; t++) {
        struct ms_dsp d; static int32_t buf[2 * 64]; double ph = 0; int32_t last = 0; long maxstep = 0;
        ms_init(&d); d.f_on = 1; d.f_type = t; d.f_res = 127; d.f_cut = 0; ms_update(&d);
        for (n = 0; n < 48000 * 4 / 64; n++) {
            d.f_cut = (uint8_t)((n * 127) / (48000 * 4 / 64)); ms_update(&d);   /* sweep the cutoff */
            for (i = 0; i < 64; i++) { buf[2 * i] = buf[2 * i + 1] = (int32_t)(5.0e8 * sin(ph)); ph += TWO_PI * 220 / FS; }
            ms_process(&d, buf, 64);
            for (i = 0; i < 64; i++) {
                long st = labs((long)buf[2 * i] - last); last = buf[2 * i];
                if (n > 4 && st > maxstep) maxstep = st;
                CHECK(labs(buf[2 * i]) <= MS_FULL, "overflow");
            }
        }
        if (maxstep > worst) worst = (double)maxstep;
        printf("type %d resonance-max cutoff sweep: largest sample step %.1f%% of full scale\n", t, 100.0 * maxstep / 2147483647.0);
        CHECK(maxstep < 0x7FFFFFFF / 2, "type %d sweep step too large (%ld)", t, maxstep);
    }
    (void)worst;
}

/* ---- 6. DC added by the EQ and filter ---- */
static void test_dc(void)
{
    struct ms_dsp d; static int32_t buf[2 * 64]; int n, i; double sum = 0, cnt = 0, ph = 0;
    ms_init(&d); d.eq_gain[0] = 60; d.eq_gain[3] = 60; d.f_on = 1; d.f_type = MS_FT_HP; d.f_cut = 20; ms_update(&d);
    for (n = 0; n < 48000 * 3 / 64; n++) {
        for (i = 0; i < 64; i++) { buf[2 * i] = buf[2 * i + 1] = (int32_t)(1.0e8 * sin(ph)); ph += TWO_PI * 1000 / FS; }
        ms_process(&d, buf, 64);
        if (n > 48000 * 2 / 64) for (i = 0; i < 64; i++) { sum += buf[2 * i]; cnt++; }
    }
    printf("mean output (DC) with a 1 kHz tone: %.0f of 2^31\n", sum / cnt);
    CHECK(fabs(sum / cnt) < 2.147e9 * 1e-4, "DC offset %.0f", sum / cnt);
}


/* ---- 7. control model: ranges, fast step, text ---- */
static void test_ui(void)
{
    struct ms_dsp d; char b[16]; int i;
    ms_init(&d);
    ms_turn(&d, MS_PAGE_FILTER, MS_KNOB_E, -1000, 0);  CHECK(d.f_cut == 0, "cutoff low clamp");
    ms_turn(&d, MS_PAGE_FILTER, MS_KNOB_E, 1000, 1);   CHECK(d.f_cut == 127, "cutoff high clamp");
    d.f_cut = 50; ms_turn(&d, MS_PAGE_FILTER, MS_KNOB_E, 1, 0);  CHECK(d.f_cut == 51, "fine step");
    ms_turn(&d, MS_PAGE_FILTER, MS_KNOB_E, 1, 1);      CHECK(d.f_cut == 59, "fast step is 8");
    ms_turn(&d, MS_PAGE_FILTER, MS_KNOB_G, 5, 0);      CHECK(d.f_type == MS_FT_BP, "type clamps at BP");
    ms_turn(&d, MS_PAGE_FILTER, MS_KNOB_G, -9, 0);     CHECK(d.f_type == MS_FT_HP, "type clamps at HP");
    ms_turn(&d, MS_PAGE_FILTER, MS_KNOB_H, 1, 0);      CHECK(d.f_on == 1 && d.filt.active, "switch on");
    ms_turn(&d, MS_PAGE_FILTER, MS_KNOB_H, -1, 0);     CHECK(d.f_on == 0 && !d.filt.active, "switch off");
    CHECK(ms_turn(&d, MS_PAGE_FILTER, MS_KNOB_A, 1, 0) == 0, "A is unused on FILTER");
    ms_turn(&d, MS_PAGE_EQ, MS_KNOB_A, 1000, 0);       CHECK(d.eq_gain[0] == 60, "gain clamps at +15");
    ms_turn(&d, MS_PAGE_EQ, MS_KNOB_B, -1000, 1);      CHECK(d.eq_gain[1] == 0, "gain clamps at -15");
    d.eq_gain[2] = 30; ms_turn(&d, MS_PAGE_EQ, MS_KNOB_C, 1, 1); CHECK(d.eq_gain[2] == 34, "fast gain step is 2 dB");
    ms_text(&d, MS_PAGE_EQ, MS_KNOB_C, b);             CHECK(!strcmp(b, "+2"), "text +2 got %s", b);
    d.eq_gain[2] = 31; ms_text(&d, MS_PAGE_EQ, MS_KNOB_C, b); CHECK(!strcmp(b, "+0.5"), "text +0.5 got %s", b);
    d.eq_gain[2] = 29; ms_text(&d, MS_PAGE_EQ, MS_KNOB_C, b); CHECK(!strcmp(b, "-0.5"), "text -0.5 got %s", b);
    d.eq_gain[2] = 30; ms_text(&d, MS_PAGE_EQ, MS_KNOB_C, b); CHECK(!strcmp(b, "0"), "text 0 got %s", b);
    d.eq_gain[0] = 0;  ms_text(&d, MS_PAGE_EQ, MS_KNOB_A, b); CHECK(!strcmp(b, "-15"), "text -15 got %s", b);
    d.eq_freq[0] = 0;  ms_text(&d, MS_PAGE_EQ, MS_KNOB_E, b); CHECK(!strcmp(b, "30"), "LS low end got %s", b);
    d.eq_freq[0] = 127; ms_text(&d, MS_PAGE_EQ, MS_KNOB_E, b); CHECK(!strcmp(b, "450"), "LS high end got %s", b);
    d.eq_freq[3] = 127; ms_text(&d, MS_PAGE_EQ, MS_KNOB_H, b); CHECK(!strcmp(b, "16k"), "HS high end got %s", b);
    d.eq_freq[1] = 0;  ms_text(&d, MS_PAGE_EQ, MS_KNOB_F, b); CHECK(!strcmp(b, "200"), "LM low end got %s", b);
    d.eq_freq[1] = 127; ms_text(&d, MS_PAGE_EQ, MS_KNOB_F, b); CHECK(!strcmp(b, "2.5k"), "LM high end got %s", b);
    d.eq_freq[2] = 0;  ms_text(&d, MS_PAGE_EQ, MS_KNOB_G, b); CHECK(!strcmp(b, "600"), "HM low end got %s", b);
    d.eq_freq[2] = 127; ms_text(&d, MS_PAGE_EQ, MS_KNOB_G, b); CHECK(!strcmp(b, "7k"), "HM high end got %s", b);
    d.f_cut = 127; ms_text(&d, MS_PAGE_FILTER, MS_KNOB_E, b); CHECK(!strcmp(b, "18k"), "filter top got %s", b);
    d.f_cut = 0;   ms_text(&d, MS_PAGE_FILTER, MS_KNOB_E, b); CHECK(!strcmp(b, "20"), "filter bottom got %s", b);
    d.f_type = MS_FT_LP; ms_text(&d, MS_PAGE_FILTER, MS_KNOB_G, b); CHECK(!strcmp(b, "LP"), "type text");
    for (i = 0; i < 8; i++) { int k; for (k = 0; k < MS_PAGES; k++) if (ms_label(k, i)) CHECK(strlen(ms_label(k, i)) <= 4, "label too long"); }
}

int main(void)
{
    test_coefs(); test_end_to_end(); test_bypass(); test_stability(); test_sweep(); test_dc(); test_ui();
    printf("sizeof(struct ms_dsp)=%zu bytes, tables=%zu bytes\n", sizeof(struct ms_dsp), sizeof(ms_a) * 2 + sizeof(ms_inv2q) + 5 * 2 * 128 * 4);
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
