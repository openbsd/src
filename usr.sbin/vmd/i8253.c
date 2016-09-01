/*
 * Copyright (c) 2016 Mike Larkin <mlarkin@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/time.h>
#include <sys/types.h>

#include <dev/ic/i8253reg.h>

#include <machine/vmmvar.h>

#include <event.h>
#include <string.h>
#include <stddef.h>

#include "i8253.h"
#include "proc.h"
#include "vmm.h"

extern char *__progname;

/*
 * Counter 0 is used to generate the legacy hardclock interrupt (HZ).
 * Counters 1 and 2 are not connected to any output (although someone
 * could hook counter 2 up to an emulated pcppi(4) at some point).
 */
struct i8253_counter i8253_counter[3];

/*
 * i8253_init
 *
 * Initialize the emulated i8253 PIT.
 *
 * Parameters:
 *  vm_id: vmm(4)-assigned ID of the VM
 */
void
i8253_init(uint32_t vm_id)
{
	memset(&i8253_counter, 0, sizeof(struct i8253_counter));
	gettimeofday(&i8253_counter[0].tv, NULL);
	i8253_counter[0].start = 0xFFFF;
	i8253_counter[0].mode = TIMER_RATEGEN;
	evtimer_set(&i8253_counter[0].timer, i8253_fire,
	    (void *)(uint64_t)vm_id);
	i8253_reset(0);
}

/*
 * vcpu_exit_i8253
 *
 * Handles emulated i8253 PIT access (in/out instruction to PIT ports).
 * We don't emulate all the modes of the i8253, just the basic squarewave
 * "rategen" clock.
 *
 * Parameters:
 *  vrp: vm run parameters containing exit information for the I/O
 *      instruction being performed
 *
 * Return value:
 *  Interrupt to inject to the guest VM, or 0xFF if no interrupt should
 *      be injected.
 */
uint8_t
vcpu_exit_i8253(struct vm_run_params *vrp)
{
	uint32_t out_data;
	uint8_t sel, rw, data, mode;
	uint64_t ns, ticks;
	struct timeval now, delta;
	union vm_exit *vei = vrp->vrp_exit;

	if (vei->vei.vei_port == TIMER_CTRL) {
		if (vei->vei.vei_dir == 0) { /* OUT instruction */
			out_data = vei->vei.vei_data;
			sel = out_data &
			    (TIMER_SEL0 | TIMER_SEL1 | TIMER_SEL2);
			sel = sel >> 6;
			if (sel > 2) {
				log_warnx("%s: i8253 PIT: unuspported "
				    "counter selected (%d)",
				    __progname, sel);
				goto ret;
			}

			rw = vei->vei.vei_data & (TIMER_LATCH | TIMER_16BIT);

			/*
			 * Since we don't truly emulate each tick of the PIT
			 * counter, when the guest asks for the timer to be
			 * latched, simulate what the counter would have been
			 * had we performed full emulation. We do this by
			 * calculating when the counter was reset vs how much
			 * time has elapsed, then bias by the counter tick
			 * rate.
			 */
			if (rw == TIMER_LATCH) {
				gettimeofday(&now, NULL);
				delta.tv_sec = now.tv_sec -
				    i8253_counter[sel].tv.tv_sec;
				delta.tv_usec = now.tv_usec -
				    i8253_counter[sel].tv.tv_usec;
				if (delta.tv_usec < 0) {
					delta.tv_sec--;
					delta.tv_usec += 1000000;
				}
				if (delta.tv_usec > 1000000) {
					delta.tv_sec++;
					delta.tv_usec -= 1000000;
				}
				ns = delta.tv_usec * 1000 +
				    delta.tv_sec * 1000000000;
				ticks = ns / NS_PER_TICK;
				i8253_counter[sel].olatch =
				    i8253_counter[sel].start -
				    ticks % i8253_counter[sel].start;
				goto ret;
			} else if (rw != TIMER_16BIT) {
				log_warnx("%s: i8253 PIT: unsupported counter "
				    "%d rw mode 0x%x selected", __func__,
				    sel, (rw & TIMER_16BIT));
			}

			goto ret;
		} else {
			log_warnx("%s: i8253 PIT: read from control port "
			    "unsupported", __progname);
			vei->vei.vei_data = 0;
		}
	} else {
		sel = vei->vei.vei_port - (TIMER_CNTR0 + TIMER_BASE);

		if (sel != 0) {
			log_warnx("%s: i8253 PIT: nonzero channel %d "
			    "selected", __func__, sel);
		}

		if (vei->vei.vei_dir == 0) { /* OUT instruction */
			if (i8253_counter[sel].last_w == 0) {
				out_data = vei->vei.vei_data;
				i8253_counter[sel].ilatch |= (out_data & 0xff);
				i8253_counter[sel].last_w = 1;
			} else {
				out_data = vei->vei.vei_data;
				i8253_counter[sel].ilatch |= ((out_data & 0xff) << 8);
				i8253_counter[sel].start =
				    i8253_counter[sel].ilatch;
				i8253_counter[sel].last_w = 0;
				mode = out_data & 0xe;

				i8253_counter[sel].mode = mode;

				i8253_reset(sel);
			}
		} else {
			if (i8253_counter[sel].last_r == 0) {
				data = i8253_counter[sel].olatch >> 8;
				vei->vei.vei_data = data;
				i8253_counter[sel].last_w = 1;
			} else {
				data = i8253_counter[sel].olatch & 0xFF;
				vei->vei.vei_data = data;
				i8253_counter[sel].last_w = 0;
			}
		}
	}

ret:
	return (0xFF);
}

/*
 * i8253_reset
 *
 * Resets the i8253's counter timer
 *
 * Parameters:
 *  chn: counter ID. Only channel ID 0 is presently emulated.
 */
void
i8253_reset(uint8_t chn)
{
	struct timeval tv;

	if (chn != 0) {
		/*
		 * Channels other than 0 are not likely to be programmed
		 * by the guest. Long ago, channel 1 was used to refresh
		 * RAM, and channel 2 is sometimes routed to the PC
		 * speaker.
		 */
		log_debug("%s: unsupported channel %d start request",
		    __func__, chn);
		return;
	}

	if (i8253_counter[chn].mode != TIMER_RATEGEN &&
	    i8253_counter[chn].mode != (TIMER_RATEGEN | 0x8)) {
		log_warnx("%s: unsupported counter mode 0x%x",
		    __func__, i8253_counter[chn].mode);
		return;
	}

	evtimer_del(&i8253_counter[chn].timer);
	timerclear(&tv);

	tv.tv_usec = (i8253_counter[chn].start * NS_PER_TICK) / 1000;
	evtimer_add(&i8253_counter[chn].timer, &tv);
}

/*
 * i8253_fire
 *
 * Callback invoked when the 8253 PIT timer fires. This will assert
 * IRQ0 on the legacy PIC attached to VCPU0.
 *
 * Parameters:
 *  fd: unused
 *  type: unused
 *  arg: VM ID
 */
void
i8253_fire(int fd, short type, void *arg)
{
	struct timeval tv;

	timerclear(&tv);
	tv.tv_usec = (i8253_counter[0].start * NS_PER_TICK) / 1000;

	vcpu_assert_pic_irq((ptrdiff_t)arg, 0, 0);
	
	evtimer_add(&i8253_counter[0].timer, &tv);
}
