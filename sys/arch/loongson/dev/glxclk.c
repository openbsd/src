/*	$OpenBSD: glxclk.c,v 1.5 2015/07/19 21:11:47 jasper Exp $	*/

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
int	glxclk_stat_intr(void *arg);
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
#define	AMD5536_MFGPT1_SCALE	0x7		/* Set to 128 */
#define	AMD5536_MFGPT1_C2_IRQM	0x00000200

#define	AMD5536_MFGPT2_CMP2	0x00000012	/* Compare value for CMP2 */
#define	AMD5536_MFGPT2_CNT	0x00000014	/* Up counter */
#define	AMD5536_MFGPT2_SETUP	0x00000016	/* Setup register */
#define	AMD5536_MFGPT2_SCALE	0x3		/* Divide by 8 */
#define	AMD5536_MFGPT2_C2_IRQM	0x00000400

#define	AMD5536_MFGPT_CNT_EN	(1 << 15)	/* Enable counting */
#define	AMD5536_MFGPT_CMP2	(1 << 14)	/* Compare 2 output */
#define	AMD5536_MFGPT_CMP1	(1 << 13)	/* Compare 1 output */
#define AMD5536_MFGPT_SETUP	(1 << 12)	/* Set to 1 after 1st write */
#define	AMD5536_MFGPT_STOP_EN	(1 << 11)	/* Stop enable */
#define	AMD5536_MFGPT_CMP2MODE	(1 << 9)|(1 << 8)/* Set to GE + activate IRQ */
#define AMD5536_MFGPT_CLKSEL	(1 << 4)	/* Clock select 14MHz */


struct glxclk_softc *glxclk_sc;

/*
 * Statistics clock interval and variance, in usec.  Variance must be a
 * power of two.  Since this gives us an even number, not an odd number,
 * we discard one case and compensate.  That is, a variance of 1024 would
 * give us offsets in [0..1023].  Instead, we take offsets in [1..1023].
 * This is symmetric about the point 512, or statvar/2, and thus averages
 * to that value (assuming uniform random numbers).
 */
/* XXX fix comment to match value */
int statvar = 8192;
int statmin;			/* statclock interval - 1/2*variance */

int
glxclk_match(struct device *parent, void *match, void *aux)
{
	struct glxpcib_attach_args *gaa = aux;
	struct cfdata *cf = match;

	if (strcmp(gaa->gaa_name, cf->cf_driver->cd_name) != 0)
		return 0;

	return 1;
}

void
glxclk_attach(struct device *parent, struct device *self, void *aux)
{
	glxclk_sc = (struct glxclk_softc *)self;
	struct glxpcib_attach_args *gaa = aux;
	u_int64_t wa;
	int statint, minint;

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

	printf(": clock");

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
	uint16_t setup = (AMD5536_MFGPT1_SCALE | AMD5536_MFGPT_CMP2MODE |
	    AMD5536_MFGPT_CMP1 | AMD5536_MFGPT_CMP2 | AMD5536_MFGPT_CNT_EN);

	bus_space_write_2(glxclk_sc->sc_iot, glxclk_sc->sc_ioh,
	    AMD5536_MFGPT1_SETUP, setup);

	/* Check to see if the MFGPT_SETUP bit was set */
	setup = bus_space_read_2(glxclk_sc->sc_iot, glxclk_sc->sc_ioh,
	    AMD5536_MFGPT1_SETUP);
	if ((setup & AMD5536_MFGPT_SETUP) == 0) {
		printf(" not configured\n");
		return;
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
	    glxclk_intr, NULL, "clock");

	md_startclock = glxclk_startclock;

	printf(", prof");


	/*
	 * Try to be as close as possible, w/o the variance, to the hardclock.
	 * The stat clock has its source set to the 14MHz clock so that the
	 * variance interval can be more generous. 
	 *
	 * Experience shows that the clock source goes crazy on scale factors
	 * lower than 8, so keep it at 8 and adjust the counter (statint) so
	 * that it results a 128Hz statclock, just like the hardclock.
	 */
	statint = 16000;
	minint = statint / 2 + 100;
	while (statvar > minint)
		statvar >>= 1;

	/* Set comparator 2 */
	bus_space_write_2(glxclk_sc->sc_iot, glxclk_sc->sc_ioh,
	    AMD5536_MFGPT2_CMP2, statint);
	statmin = statint - (statvar >> 1);

	/* Reset counter to 0 */
	bus_space_write_2(glxclk_sc->sc_iot, glxclk_sc->sc_ioh,
	    AMD5536_MFGPT2_CNT, 0);

	/*
	 * All the bits in the range 11:0 have to be written at once.
	 * After they're set the first time all further writes are
	 * ignored.
	 */
	setup = (AMD5536_MFGPT2_SCALE | AMD5536_MFGPT_CMP2MODE |
	    AMD5536_MFGPT_CLKSEL | AMD5536_MFGPT_CMP1 | AMD5536_MFGPT_CMP2 |
	    AMD5536_MFGPT_CNT_EN);

	bus_space_write_2(glxclk_sc->sc_iot, glxclk_sc->sc_ioh,
	    AMD5536_MFGPT2_SETUP, setup);

	/* Check to see if the MFGPT_SETUP bit was set */
	setup = bus_space_read_2(glxclk_sc->sc_iot, glxclk_sc->sc_ioh,
	    AMD5536_MFGPT2_SETUP);
	if ((setup & AMD5536_MFGPT_SETUP) == 0) {
		printf(" not configured\n");
		return;
	}

	/* Enable MFGPT2 Comparator 2 Output to the Interrupt Mapper */
	wa = rdmsr(MFGPT_IRQ);
	wa |= AMD5536_MFGPT2_C2_IRQM;
	wrmsr(MFGPT_IRQ, wa);

	/*
	 * Tie PIC input 6 to IG8 for glxstat(4).
	 */
	wa = rdmsr(PIC_ZSEL_LOW);
	wa &= ~(0xfUL << 24);
	wa |= 8 << 24;
	wrmsr(PIC_ZSEL_LOW, wa);

	/*
	 * The interrupt argument is NULL in order to notify the dispatcher
	 * to pass the clock frame as argument. This trick also forces keeping
	 * the soft state global because during the interrupt we need to clear
	 * the comp2 event in the MFGPT setup register.
	 */
	isa_intr_establish(sys_platform->isa_chipset, 8, IST_PULSE,
	    IPL_STATCLOCK, glxclk_stat_intr, NULL, "prof");

	printf("\n");
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

int
glxclk_stat_intr(void *arg)
{
	struct clockframe *frame = arg;
	uint16_t setup = 0;
	struct cpu_info *ci = curcpu();
	u_long newint, r, var;

	/* Clear the current event */
	setup = bus_space_read_2(glxclk_sc->sc_iot, glxclk_sc->sc_ioh,
	    AMD5536_MFGPT2_SETUP);
	setup |= AMD5536_MFGPT_CMP2;
	bus_space_write_2(glxclk_sc->sc_iot, glxclk_sc->sc_ioh,
	    AMD5536_MFGPT2_SETUP, setup);

	if (ci->ci_clock_started == 0)
		return 1;

	statclock(frame);

	/*
	 * Compute new randomized interval.  The intervals are uniformly
	 * distributed on [statint - statvar / 2, statint + statvar / 2],
	 * and therefore have mean statint, giving a stathz frequency clock.
	 */
	var = statvar;
	do {
		r = random() & (var - 1);
	} while (r == 0);
	newint = statmin + r;

	bus_space_write_2(glxclk_sc->sc_iot, glxclk_sc->sc_ioh,
	    AMD5536_MFGPT2_CMP2, newint);

	return 1;
}
