/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Master Suite control model: which knob does what on which page, the
 * press-and-turn fast edit, and the text each value shows. No drawing here:
 * drawing and key/encoder wiring depend on OS addresses (see main.c). */
#ifndef MS_UI_H
#define MS_UI_H
#include "ms_dsp.h"

enum { MS_PAGE_FILTER, MS_PAGE_EQ, MS_PAGES };
enum { MS_KNOB_A, MS_KNOB_B, MS_KNOB_C, MS_KNOB_D, MS_KNOB_E, MS_KNOB_F, MS_KNOB_G, MS_KNOB_H };

/* Turn knob `knob` of `page` by `delta` detents. `fast` is the press-and-turn
 * (or FUNC) coarse step. Returns 1 if the knob does something on that page. */
int ms_turn(struct ms_dsp *d, int page, int knob, int delta, int fast);
/* The current value as short text (at most 7 chars + NUL); returns 0 if unused. */
int ms_text(const struct ms_dsp *d, int page, int knob, char *out);
/* The knob's label on that page ("CUT", "LS", ...); NULL if unused. */
const char *ms_label(int page, int knob);
#endif
