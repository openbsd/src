/*	$OpenBSD: led.c,v 1.1 1998/06/04 03:49:48 jason Exp $	*/

/*
 * Copyright (c) 1998 Jason L. Wright (jason@thought.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <pmax/pmax/kn01.h>
#include <pmax/pmax/pmaxtype.h>
#include <pmax/dev/led.h>

/*
 * This is the driver for the "led" register available on the kn01 machines.
 */

static int ledmatch __P((struct device *, void *, void *));
static void ledattach __P((struct device *, struct device *, void *));

struct cfattach led_ca = {
	sizeof(struct device), ledmatch, ledattach
};

struct cfdriver led_cd = {
	NULL, "led", DV_DULL
};

/* from machdep */
extern int pmax_boardtype;

static char led_attached = 0;
static int led_index = 0;
u_short led_current = 0xff;
int pmax_led_blink;

/*
 * These led patterns produce a line that scrolls across the display, then
 * back again.  Note that a value of 0 for a particular bit lights the
 * corresponding LED, and 1 leaves it dark.
 */
static char led_patterns[] =
	{ 0xff, 0x7f, 0xbf, 0xdf, 0xef, 0xf7, 0xfb, 0xfd, 0xfe,
	  0xff, 0xfe, 0xfd, 0xfb, 0xf7, 0xef, 0xdf, 0xbf, 0x7f, };

static int
ledmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct confargs *ca = aux;

	if (pmax_boardtype == DS_PMAX)		/* only handle [23]100 now */
		return (strcmp("led", ca->ca_name) == 0);
	return (0);
}

/* ARGSUSED */
static void
ledattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	led_attached = 1;
	printf("\n");

	/* In case it's initialized to true... */
	if (pmax_led_blink)
		led_pmax_cycle((caddr_t)0);
}

/*
 * Check to see whether we were configured and whether machdep.led_blink != 0.
 * If so, put a new pattern into the register and schedule ourselves to
 * be called again later.  The timeout is set to: [(1/8) * loadavg] seconds.
 */
void
led_pmax_cycle(zero)
	void *zero;
{
	int s;
	unsigned short csr;

	if (!led_attached)
		return;

	if (!pmax_led_blink)
		led_current = 0xff;
	else {
		led_index = (led_index + 1) % sizeof(led_patterns);
		led_current = led_patterns[led_index] & KN01_CSR_LEDS_MASK;
	}

	csr = *((volatile u_short *) MIPS_PHYS_TO_KSEG1(KN01_SYS_CSR));
	csr = (csr & ~KN01_CSR_LEDS_MASK) | led_current;
	*((volatile u_short *) MIPS_PHYS_TO_KSEG1(KN01_SYS_CSR)) = csr;

	if (pmax_led_blink) {
		s = (((averunnable.ldavg[0] + FSCALE) * hz) >> (FSHIFT + 3));
		timeout(led_pmax_cycle, (caddr_t)0, s);
	}
}
