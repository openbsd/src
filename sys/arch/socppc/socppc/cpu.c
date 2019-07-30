/*	$OpenBSD: cpu.c,v 1.9 2015/03/31 15:51:05 mpi Exp $	*/

/*
 * Copyright (c) 2008 Mark Kettenis
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/rndvar.h>

#include <machine/autoconf.h>
#include <powerpc/hid.h>

#include <dev/ofw/openfirm.h>


struct cpu_info cpu_info[PPC_MAXPROCS] = { { NULL } }; /* XXX */

char cpu_model[] = "8347";
char machine[] = MACHINE;	/* cpu architecture */

int	cpu_match(struct device *, void *, void *);
void	cpu_attach(struct device *, struct device *, void *);

struct cfattach cpu_ca = {
	sizeof(struct device), cpu_match, cpu_attach
};

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL
};

int
cpu_match(struct device *parent, void *cfdata, void *aux)
{
	struct mainbus_attach_args *ma = aux;
	char buf[32];

	if (OF_getprop(ma->ma_node, "device_type", buf, sizeof(buf)) <= 0)
		return (0);

	if (strcmp(buf, "cpu") == 0)
		return (1);

	return (0);
}

void
cpu_attach(struct device *parent, struct device *self, void *aux)
{
	struct cpu_info *ci;
	u_int32_t hid0;

	ci = &cpu_info[0];
	ci->ci_cpuid = 0;
	ci->ci_intrdepth = -1;
	ci->ci_dev = self;

	printf(": %s\n", cpu_model);

	/* Enable data cache. */
	hid0 = ppc_mfhid0();
	if ((hid0 & HID0_DCE) == 0) {
		__asm volatile (
		    "sync; mtspr 1008,%0; mtspr 1008,%1"
		    :: "r" (hid0 | HID0_DCFI), "r" (hid0 | HID0_DCE));
	}

	/* Enable instruction cache. */
	hid0 = ppc_mfhid0();
	if ((hid0 & HID0_ICE) == 0) {
		__asm volatile (
		    "isync; mtspr 1008,%0; mtspr 1008,%1"
		    :: "r" (hid0 | HID0_ICFI), "r" (hid0 | HID0_ICE));
	}

	/* Select DOZE mode. */
	hid0 = ppc_mfhid0();
	hid0 &= ~(HID0_NAP | HID0_DOZE | HID0_SLEEP);
	hid0 |= HID0_DOZE | HID0_DPM;
	ppc_mthid0(hid0);
	ppc_cpuidle = 1;
}
