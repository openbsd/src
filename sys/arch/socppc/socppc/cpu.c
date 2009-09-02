/*	$OpenBSD: cpu.c,v 1.4 2009/09/02 20:29:39 kettenis Exp $	*/

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

#include <machine/autoconf.h>

#include <dev/ofw/openfirm.h>

#define HID0_DOZE	(1 << (31-8))
#define HID0_NAP	(1 << (31-9))
#define HID0_SLEEP	(1 << (31-10))
#define HID0_DPM	(1 << (31-11))
#define HID0_ICE	(1 << (31-16))
#define HID0_DCE	(1 << (31-17))
#define HID0_ICFI	(1 << (31-20))
#define HID0_DCFI	(1 << (31-21))

extern u_int32_t	hid0_idle;


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
	ci->ci_randseed = 1;
	ci->ci_dev = self;

	printf(": %s\n", cpu_model);

	/* Enable data cache. */
	hid0 = ppc_mfhid0();
	if ((hid0 & HID0_DCE) == 0) {
		__asm __volatile (
		    "sync; mtspr 1008,%0; mtspr 1008,%1"
		    :: "r" (hid0 | HID0_DCFI), "r" (hid0 | HID0_DCE));
	}

	/* Enable instruction cache. */
	hid0 = ppc_mfhid0();
	if ((hid0 & HID0_ICE) == 0) {
		__asm __volatile (
		    "isync; mtspr 1008,%0; mtspr 1008,%1"
		    :: "r" (hid0 | HID0_ICFI), "r" (hid0 | HID0_ICE));
	}

	/* Select DOZE mode. */
	hid0 = ppc_mfhid0();
	hid0 &= ~(HID0_NAP | HID0_DOZE | HID0_SLEEP);
	hid0_idle = HID0_DOZE;
	hid0 |= HID0_DPM;
	ppc_mthid0(hid0);
}

int ppc_proc_is_64b;
extern u_int32_t nop_inst;
struct patch {
	u_int32_t *s;
	u_int32_t *e;
};
extern struct patch nop32_start;

void
ppc_check_procid()
{
	u_int32_t *inst;
	struct patch *p;

	ppc_proc_is_64b = 0;
	for (p = &nop32_start; p->s; p++) {
		for (inst = p->s; inst < p->e; inst++)
			*inst = nop_inst;
		syncicache(p->s, (p->e - p->s) * sizeof(*p->e));
	}
}
