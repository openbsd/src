/*	$OpenBSD: glxclk.c,v 1.1 2013/01/14 21:18:47 pirofti Exp $	*/

/*
 * Copyright (c) 2013 Paul Irofti.
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
#include <sys/proc.h>

#include <machine/bus.h>
#include <machine/autoconf.h>

#include <dev/isa/isavar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/glxreg.h>
#include <dev/pci/glxvar.h>

struct glxclk_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct cfdriver glxclk_cd = {
	NULL, "glxclk", DV_DULL
};

int	glxclk_match(struct device *, void *, void *);
void	glxclk_attach(struct device *, struct device *, void *);
int	glxclk_intr(void *);
void	glxclk_startclock(struct cpu_info *);

struct cfattach glxclk_ca = {
	sizeof(struct glxclk_softc), glxclk_match, glxclk_attach,
};

#define	MSR_LBAR_ENABLE		0x100000000ULL
#define	MSR_LBAR_MFGPT		DIVIL_LBAR_MFGPT
#define	MSR_MFGPT_SIZE		0x40
#define	MSR_MFGPT_ADDR_MASK	0xffc0

#define	AMD5536_MFGPT1_CMP2	0x0000000a	/* Compare value for CMP2 */
#define	AMD5536_MFGPT1_CNT	0x0000000c	/* Up counter */
#define	AMD5536_MFGPT1_SETUP	0x0000000e	/* Setup register */

#define	AMD5536_MFGPT_CNT_EN	(1 << 15)	/* Enable counting */
#define	AMD5536_MFGPT_CMP2	(1 << 14)	/* Compare 2 output */
#define	AMD5536_MFGPT_CMP1	(1 << 13)	/* Compare 1 output */
#define AMD5536_MFGPT_SETUP	(1 << 12)	/* Set to 1 after 1st write */
#define	AMD5536_MFGPT_STOP_EN	(1 << 11)	/* Stop enable */
#define	AMD5536_MFGPT_CMP2MODE	(1 << 9)|(1 << 8)/* Set to GE + activate IRQ */
#define	AMD5536_MFGPT_SCALE	0x7		/* Set to 128 */
#define AMD5536_MFGPT_CLKSEL	(1 << 4)	/* Clock select 14MHz */

#define AMD5536_MFGPT1_C2_NMIM	(1 << 9)	/* Enable NMIs for MFGPT1 */
#define	AMD5536_MFGPT1_C2_RSTEN	0x02000000
#define	AMD5536_MFGPT1_C2_IRQM	0x00000200
#define	AMD5536_MFGPT5_C2_IRQM	0x00002000

struct glxclk_softc *glxclk_sc;

int
glxclk_match(struct device *parent, void *match, void *aux)
{
	struct glxpcib_attach_args *gaa = aux;
	struct cfdata *cf = match;

	if (gaa->gaa_name == NULL || strcmp(gaa->gaa_name,
	    cf->cf_driver->cd_name) != 0)
		return 0;

	return 1;
}

void
glxclk_attach(struct device *parent, struct device *self, void *aux)
{
	glxclk_sc = (struct glxclk_softc *)self;
	struct glxpcib_attach_args *gaa = aux;
	u_int64_t wa;

	glxclk_sc->sc_iot = gaa->gaa_iot;
	glxclk_sc->sc_ioh = gaa->gaa_ioh;

	wa = rdmsr(MSR_LBAR_MFGPT);

	if ((wa & MSR_LBAR_ENABLE) == 0) {
		printf(" not configured\n");
		return;
	}

	if (bus_space_map(glxclk_sc->sc_iot, wa & MSR_MFGPT_ADDR_MASK,
	    MSR_MFGPT_SIZE, 0, &glxclk_sc->sc_ioh)) {
		printf(" not configured\n");
		return;
	}

	/* Set comparator 2 */
	bus_space_write_2(glxclk_sc->sc_iot, glxclk_sc->sc_ioh,
	    AMD5536_MFGPT1_CMP2, 1);

	/* Reset counter to 0 */
	bus_space_write_2(glxclk_sc->sc_iot, glxclk_sc->sc_ioh,
	    AMD5536_MFGPT1_CNT, 0);

	/*
	 * All the bits in the range 11:0 have to be written at once.
	 * After they're set the first time all further writes are
	 * ignored.
	 */
	uint16_t setup = (AMD5536_MFGPT_SCALE | AMD5536_MFGPT_CMP2MODE |
			AMD5536_MFGPT_CMP1 | AMD5536_MFGPT_CMP2 |
			AMD5536_MFGPT_CNT_EN);

	bus_space_write_2(glxclk_sc->sc_iot, glxclk_sc->sc_ioh,
	    AMD5536_MFGPT1_SETUP, setup);

	/* Check to see if the MFGPT_SETUP bit was set */
	setup = bus_space_read_2(glxclk_sc->sc_iot, glxclk_sc->sc_ioh,
	    AMD5536_MFGPT1_SETUP);
	if ((setup & AMD5536_MFGPT_SETUP) == 0) {
		printf(" not configured\n");
	}

	/* Enable MFGPT1 Comparator 2 Output to the Interrupt Mapper */
	wa = rdmsr(MFGPT_IRQ);
	wa |= AMD5536_MFGPT1_C2_IRQM;
	wrmsr(MFGPT_IRQ, wa);

	/*
	 * Tie PIC input 5 to IG7 for glxclk(4).
	 */
	wa = rdmsr(PIC_ZSEL_LOW);
	wa &= ~(0xfUL << 20);
	wa |= 7 << 20;
	wrmsr(PIC_ZSEL_LOW, wa);

	/* Start the counter */
	setup = (AMD5536_MFGPT_CNT_EN | AMD5536_MFGPT_CMP2);
	bus_space_write_2(glxclk_sc->sc_iot, glxclk_sc->sc_ioh,
	    AMD5536_MFGPT1_SETUP, setup);

	/*
	 * The interrupt argument is NULL in order to notify the dispatcher
	 * to pass the clock frame as argument. This trick also forces keeping
	 * the soft state global because during the interrupt we need to clear
	 * the comp2 event in the MFGPT setup register.
	 */
	isa_intr_establish(sys_platform->isa_chipset, 7, IST_PULSE, IPL_CLOCK,
	    glxclk_intr, NULL, self->dv_xname);

	md_startclock = glxclk_startclock;

	printf(": MFGPT1\n");
}

void
glxclk_startclock(struct cpu_info *ci)
{
	/* Start the clock. */
	int s = splclock();
	ci->ci_clock_started++;
	splx(s);
}

int
glxclk_intr(void *arg)
{
	struct clockframe *frame = arg;
	uint16_t setup = 0;
	struct cpu_info *ci = curcpu();

	/* Clear the current event */
	setup = bus_space_read_2(glxclk_sc->sc_iot, glxclk_sc->sc_ioh,
	    AMD5536_MFGPT1_SETUP);
	setup |= AMD5536_MFGPT_CMP2;
	bus_space_write_2(glxclk_sc->sc_iot, glxclk_sc->sc_ioh,
	    AMD5536_MFGPT1_SETUP, setup);

	if (ci->ci_clock_started == 0)
		return 1;

	hardclock(frame);

	return 1;
}
/*	$OpenBSD: glxclk.c,v 1.1 2013/01/14 21:18:47 pirofti Exp $	*/

/*
 * Copyright (c) 2010 Paul Irofti.
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

#include <machine/bus.h>
#include <machine/autoconf.h>

#include <dev/isa/isavar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/glxreg.h>
#include <dev/pci/glxvar.h>

struct glxclk_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct cfdriver glxclk_cd = {
	NULL, "glxclk", DV_DULL
};

int	glxclk_match(struct device *, void *, void *);
void	glxclk_attach(struct device *, struct device *, void *);
int	glxclk_activate(struct device *, int);
int	glxclk_intr(void *);

struct cfattach glxclk_ca = {
	sizeof(struct glxclk_softc), glxclk_match, glxclk_attach,
	NULL, glxclk_activate
};

#define	MSR_LBAR_ENABLE		0x100000000ULL
#define	MSR_LBAR_MFGPT		DIVIL_LBAR_MFGPT
#define	MSR_MFGPT_SIZE		0x40
#define	MSR_MFGPT_ADDR_MASK	0xffc0

#define	AMD5536_MFGPT1_CMP2	0x0000000a	/* Compare value for CMP2 */
#define	AMD5536_MFGPT1_CNT	0x0000000c	/* Up counter */
#define	AMD5536_MFGPT1_SETUP	0x0000000e	/* Setup register */

#define	AMD5536_MFGPT_CNT_EN	(1 << 15)	/* Enable counting */
#define	AMD5536_MFGPT_CMP2	(1 << 14)	/* Compare 2 output */
#define	AMD5536_MFGPT_CMP1	(1 << 13)	/* Compare 1 output */
#define AMD5536_MFGPT_SETUP	(1 << 12)	/* Set to 1 after 1st write */
#define	AMD5536_MFGPT_STOP_EN	(1 << 11)	/* Stop enable */
#define	AMD5536_MFGPT_CMP2MODE	(1 << 9)|(1 << 8)/* Set to GE + activate IRQ */
#define	AMD5536_MFGPT_SCALE	0x0f		/* Set to 32768 */
#define AMD5536_MFGPT_CLKSEL	(1 << 4)	/* Clock select 14MHz */

#define AMD5536_MFGPT1_C2_NMIM	(1 << 9)	/* Enable NMIs for MFGPT1 */
#define	AMD5536_MFGPT1_C2_RSTEN	0x02000000
#define	AMD5536_MFGPT1_C2_IRQM	0x00000200
#define	AMD5536_MFGPT5_C2_IRQM	0x00002000

/* XXX: overflow */
#define MFGPT_TICK 14318000
#define COMP2	((MFGPT_TICK + HZ / 2) / HZ)

int
glxclk_match(struct device *parent, void *match, void *aux)
{
	struct glxpcib_attach_args *gaa = aux;
	struct cfdata *cf = match;

	if (gaa->gaa_name == NULL || strcmp(gaa->gaa_name,
	    cf->cf_driver->cd_name) != 0)
		return 0;

	return 1;
}

void
glxclk_attach(struct device *parent, struct device *self, void *aux)
{
	struct glxclk_softc *sc = (struct glxclk_softc *)self;
	struct glxpcib_attach_args *gaa = aux;
	u_int64_t wa;

	sc->sc_iot = gaa->gaa_iot;
	sc->sc_ioh = gaa->gaa_ioh;

	wa = rdmsr(MSR_LBAR_MFGPT);
	
	if ((wa & MSR_LBAR_ENABLE) == 0) {
		printf(" not configured\n");
		return;
	}
	
	if (bus_space_map(sc->sc_iot, wa & MSR_MFGPT_ADDR_MASK,
	    MSR_MFGPT_SIZE, 0, &sc->sc_ioh)) {
		printf(" not configured\n");
		return;
	}

	/* Set comparator 2 */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMD5536_MFGPT1_CMP2, 0xffff);

	/* Reset counter to 0 */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMD5536_MFGPT1_CNT, 0);

	/* count in seconds (as upper level desires) */

	/* 
	 * All the bits in the range 11:0 have to be written at once.
	 * After they're set the first time all further writes are
	 * ignored.
	 */
	uint16_t setup = (AMD5536_MFGPT_CLKSEL | AMD5536_MFGPT_CMP2MODE |
	    AMD5536_MFGPT_CMP1 | AMD5536_MFGPT_CMP2 | AMD5536_MFGPT_CNT_EN);

	bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMD5536_MFGPT1_SETUP, setup);

	/* Check to see if the MFGPT_SETUP bit was set */
	setup = bus_space_read_2(sc->sc_iot, sc->sc_ioh, AMD5536_MFGPT1_SETUP); 
	if ((setup & AMD5536_MFGPT_SETUP) == 0) {
		printf(" not configured\n");
	}	

#if 0
	/* Enable NMI's for MFGPT1 */
	wa = rdmsr(MFGPT_NR);
	wa |= AMD5536_MFGPT1_C2_RSTEN;
	wrmsr(MFGPT_NR, wa);
#endif

	/* Enable MFGPT1 Comparator 2 Output to the Interrupt Mapper */
	wa = rdmsr(MFGPT_IRQ);
	wa |= AMD5536_MFGPT1_C2_IRQM;
	wrmsr(MFGPT_IRQ, wa);

	/*
	 * Tie PIC input 5 to IG5 for glxclk(4).
	 */
	wa = rdmsr(PIC_ZSEL_LOW);
	wa &= ~(0xfUL << 20);
	wa |= 7 << 20;
	wrmsr(PIC_ZSEL_LOW, wa);

	/* Start the counter */
	setup = (AMD5536_MFGPT_CNT_EN | AMD5536_MFGPT_CMP2);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMD5536_MFGPT1_SETUP, setup);

	isa_intr_establish(sys_platform->isa_chipset, 7, IST_PULSE, IPL_CLOCK,
	    glxclk_intr, sc, self->dv_xname);

	printf(": MFGPT1\n");
}

int
glxclk_intr(void *arg)
{
	struct glxclk_softc *sc = (struct glxclk_softc *)arg;
	uint16_t setup = 0;

	/* Clear the current event */
	setup = bus_space_read_2(sc->sc_iot, sc->sc_ioh, AMD5536_MFGPT1_SETUP); 
	setup |= AMD5536_MFGPT_CMP2;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, AMD5536_MFGPT1_SETUP, setup);

	return 1;
}

int
glxclk_activate(struct device *self, int act)
{
	return 0;
}
