/*      $OpenBSD: glxpcib.c,v 1.2 2007/10/07 18:41:07 mbalmer Exp $	*/

/*
 * Copyright (c) 2007 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * AMD CS5536 series LPC bridge also containing timer and watchdog
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/sysctl.h>
#include <sys/timetc.h>

#include <machine/bus.h>
#include <machine/cpufunc.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define	AMD5536_REV		0x51400017
#define	AMD5536_REV_MASK	0xff
#define	AMD5536_TMC		0x51400050

#define	MSR_LBAR_MFGPT		0x5140000d
#define	AMD5536_MFGPT0_CMP1	0x00000000
#define	AMD5536_MFGPT0_CMP2	0x00000002
#define	AMD5536_MFGPT0_CNT	0x00000004
#define	AMD5536_MFGPT0_SETUP	0x00000006
#define	AMD5536_MFGPT_DIV_MASK	0x000f	/* div = 1 << mask */
#define	AMD5536_MFGPT_CLKSEL	0x0010
#define	AMD5536_MFGPT_REV_EN	0x0020
#define	AMD5536_MFGPT_CMP1DIS	0x0000
#define	AMD5536_MFGPT_CMP1EQ	0x0040
#define	AMD5536_MFGPT_CMP1GE	0x0080
#define	AMD5536_MFGPT_CMP1EV	0x00c0
#define	AMD5536_MFGPT_CMP2DIS	0x0000
#define	AMD5536_MFGPT_CMP2EQ	0x0100
#define	AMD5536_MFGPT_CMP2GE	0x0200
#define	AMD5536_MFGPT_CMP2EV	0x0300
#define	AMD5536_MFGPT_STOP_EN	0x0800
#define	AMD5536_MFGPT_SET	0x1000
#define	AMD5536_MFGPT_CMP1	0x2000
#define	AMD5536_MFGPT_CMP2	0x4000
#define	AMD5536_MFGPT_CNT_EN	0x8000
#define	AMD5536_MFGPT_IRQ	0x51400028
#define	AMD5536_MFGPT0_C1_IRQM	0x00000001
#define	AMD5536_MFGPT1_C1_IRQM	0x00000002
#define	AMD5536_MFGPT2_C1_IRQM	0x00000004
#define	AMD5536_MFGPT3_C1_IRQM	0x00000008
#define	AMD5536_MFGPT4_C1_IRQM	0x00000010
#define	AMD5536_MFGPT5_C1_IRQM	0x00000020
#define	AMD5536_MFGPT6_C1_IRQM	0x00000040
#define	AMD5536_MFGPT7_C1_IRQM	0x00000080
#define	AMD5536_MFGPT0_C2_IRQM	0x00000100
#define	AMD5536_MFGPT1_C2_IRQM	0x00000200
#define	AMD5536_MFGPT2_C2_IRQM	0x00000400
#define	AMD5536_MFGPT3_C2_IRQM	0x00000800
#define	AMD5536_MFGPT4_C2_IRQM	0x00001000
#define	AMD5536_MFGPT5_C2_IRQM	0x00002000
#define	AMD5536_MFGPT6_C2_IRQM	0x00004000
#define	AMD5536_MFGPT7_C2_IRQM	0x00008000
#define	AMD5536_MFGPT_NR	0x51400029
#define	AMD5536_MFGPT0_C1_NMIM	0x00000001
#define	AMD5536_MFGPT1_C1_NMIM	0x00000002
#define	AMD5536_MFGPT2_C1_NMIM	0x00000004
#define	AMD5536_MFGPT3_C1_NMIM	0x00000008
#define	AMD5536_MFGPT4_C1_NMIM	0x00000010
#define	AMD5536_MFGPT5_C1_NMIM	0x00000020
#define	AMD5536_MFGPT6_C1_NMIM	0x00000040
#define	AMD5536_MFGPT7_C1_NMIM	0x00000080
#define	AMD5536_MFGPT0_C2_NMIM	0x00000100
#define	AMD5536_MFGPT1_C2_NMIM	0x00000200
#define	AMD5536_MFGPT2_C2_NMIM	0x00000400
#define	AMD5536_MFGPT3_C2_NMIM	0x00000800
#define	AMD5536_MFGPT4_C2_NMIM	0x00001000
#define	AMD5536_MFGPT5_C2_NMIM	0x00002000
#define	AMD5536_MFGPT6_C2_NMIM	0x00004000
#define	AMD5536_MFGPT7_C2_NMIM	0x00008000
#define	AMD5536_NMI_LEG		0x00010000
#define	AMD5536_MFGPT0_C2_RSTEN	0x01000000
#define	AMD5536_MFGPT1_C2_RSTEN	0x02000000
#define	AMD5536_MFGPT2_C2_RSTEN	0x04000000
#define	AMD5536_MFGPT3_C2_RSTEN	0x08000000
#define	AMD5536_MFGPT4_C2_RSTEN	0x10000000
#define	AMD5536_MFGPT5_C2_RSTEN	0x20000000
#define	AMD5536_MFGPT_SETUP	0x5140002b

struct glxpcib_softc {
	struct device		sc_dev;

	struct timecounter	sc_timecounter;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct cfdriver glxpcib_cd = {
	NULL, "glxpcib", DV_DULL
};

int	glxpcib_match(struct device *, void *, void *);
void	glxpcib_attach(struct device *, struct device *, void *);

struct cfattach glxpcib_ca = {
	sizeof(struct glxpcib_softc), glxpcib_match, glxpcib_attach
};

/* from arch/<*>/pci/pcib.c */
void	pcibattach(struct device *parent, struct device *self, void *aux);

u_int	glxpcib_get_timecount(struct timecounter *tc);
#ifndef SMALL_KERNEL
int     glxpcib_wdogctl_cb(void *, int);
#endif

const struct pci_matchid glxpcib_devices[] = {
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_CS5536_PCIB }
};

int
glxpcib_match(struct device *parent, void *match, void *aux)
{ 
	if (pci_matchbyid((struct pci_attach_args *)aux, glxpcib_devices,
	    sizeof(glxpcib_devices) / sizeof(glxpcib_devices[0])))
		return 2;

	return 0;
}

void
glxpcib_attach(struct device *parent, struct device *self, void *aux)
{
	struct glxpcib_softc *sc = (struct glxpcib_softc *)self;
	struct timecounter *tc = &sc->sc_timecounter;
#ifndef SMALL_KERNEL
	struct pci_attach_args *pa = aux;
	u_int64_t wa;
#endif

	tc->tc_get_timecount = glxpcib_get_timecount;
	tc->tc_counter_mask = 0xffffffff;
	tc->tc_frequency = 3579545;
	tc->tc_name = "CS5536";
	tc->tc_quality = 1000;
	tc->tc_priv = sc;
	tc_init(tc);

	printf(": rev %d, 32-bit %lluHz timer",
	    (int)rdmsr(AMD5536_REV) & AMD5536_REV_MASK,
	    tc->tc_frequency);

#ifndef SMALL_KERNEL
	sc->sc_iot = pa->pa_iot;
	wa = rdmsr(MSR_LBAR_MFGPT);
	if (wa & 0x100000000ULL &&
	    !bus_space_map(sc->sc_iot, wa & 0xffff, 64, 0, &sc->sc_ioh)) {

		/* count in seconds (as upper level desires) */
		bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMD5536_MFGPT0_SETUP,
		    AMD5536_MFGPT_CNT_EN | AMD5536_MFGPT_CMP2EV |
		    AMD5536_MFGPT_CMP2 | AMD5536_MFGPT_DIV_MASK);
		wdog_register(sc, glxpcib_wdogctl_cb);

		printf(", watchdog");
	}
#endif
	pcibattach(parent, self, aux);
}

u_int
glxpcib_get_timecount(struct timecounter *tc)
{
        return rdmsr(AMD5536_TMC);
}

#ifndef SMALL_KERNEL
int
glxpcib_wdogctl_cb(void *v, int period)
{
	struct glxpcib_softc *sc = v;

	if (period > 0xffff)
		period = 0xffff;

	bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMD5536_MFGPT0_SETUP,
	    AMD5536_MFGPT_CNT_EN | AMD5536_MFGPT_CMP2);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMD5536_MFGPT0_CNT, 0);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMD5536_MFGPT0_CMP2, period);

	if (period)
		wrmsr(AMD5536_MFGPT_NR,
		    rdmsr(AMD5536_MFGPT_NR) | AMD5536_MFGPT0_C2_RSTEN);
	else
		wrmsr(AMD5536_MFGPT_NR,
		    rdmsr(AMD5536_MFGPT_NR) & ~AMD5536_MFGPT0_C2_RSTEN);

	return (period);
}
#endif
