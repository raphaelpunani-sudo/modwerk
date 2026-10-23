/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Master Suite for the Digitone: filter and 4-band EQ on the master mix.
 *
 * STATUS: the DSP (ms_dsp.c) and the control model (ms_ui.c) are written and
 * tested on a host. What is NOT written, because it needs OS 1.44 addresses
 * that come from the owner's own firmware (never committed):
 *   - MS_MIX_ADDR / MS_MIX_FRAMES below: where the render leaves the master
 *     mix and how many frames a block has, and its sample format;
 *   - the page: registering the mod page, drawing it (text and rectangle
 *     routines) and reading the knob/encoder events.
 * Until MS_MIX_ADDR is set, the render handler does nothing, so a build of
 * this module is silent and safe. Nothing here has run on a Digitone.
 */
#include <stdint.h>
#include "ms_dsp.h"
#include "ms_ui.h"

/* ---- TO FILL IN from your own OS 1.44 (see TESTING.md, "Open items") ---- */
#ifndef MS_MIX_ADDR
#define MS_MIX_ADDR 0u          /* address of the interleaved stereo int32 master mix */
#endif
#ifndef MS_MIX_FRAMES
#define MS_MIX_FRAMES 0         /* frames per render block at that address */
#endif

struct ms_dsp ms_state;         /* global, so tests and the page code can reach it */
static uint8_t ms_ready;

/* ev_render_out: after the OS has rendered a block. */
void ms_render_out(void)
{
    if (!MS_MIX_ADDR || MS_MIX_FRAMES <= 0) return;
    if (!ms_ready) { ms_init(&ms_state); ms_ready = 1; }
    ms_process(&ms_state, (int32_t *)(uintptr_t)MS_MIX_ADDR, MS_MIX_FRAMES);
}
