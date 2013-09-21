/*	$OpenBSD: dart.c,v 1.61 2013/09/21 20:05:01 miod Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
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
/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/board.h>
#include <machine/conf.h>
#include <machine/cpu.h>

#include <dev/cons.h>

#include <dev/ic/mc68681reg.h>
#include <dev/ic/mc68681var.h>

#include <machine/mvme181.h>
#include <machine/mvme188.h>

struct dartsoftc {
	struct mc68681_softc	sc_base;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	struct intrhand		sc_ih;
};

int	dartmatch(struct device *parent, void *self, void *aux);
void	dartattach(struct device *parent, struct device *self, void *aux);

struct cfattach dart_ca = {
	sizeof(struct dartsoftc), dartmatch, dartattach
};

/* console, if applicable, will always be on the first port */
#define	CONS_PORT	A_PORT

cons_decl(dart);
int	dartintr(void *);
uint8_t	dart_read(void *, uint);
void	dart_write(void *, uint, uint8_t);

/*
 * DUART registers are mapped as the least-significant byte of 32-bit addresses.
 * The MVME188 documentation recommends using 32-bit accesses.
 */

uint8_t
dart_read(void *v, uint reg)
{
	struct dartsoftc *sc = v;

	return (uint8_t)bus_space_read_4(sc->sc_iot, sc->sc_ioh, reg << 2);
}

void
dart_write(void *v, uint reg, uint8_t val)
{
	struct dartsoftc *sc = v;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, reg << 2, val);
	/*
	 * Multiple accesses to the same command register must be delayed,
	 * to prevent the chip from misbehaving.
	 */
	if (reg == DART_CRA || reg == DART_CRB)
		DELAY(2);
}

int
dartmatch(struct device *parent, void *cf, void *aux)
{
	struct confargs *ca = aux;
	bus_space_handle_t ioh;
	int rc;

	/*
	 * We do not accept empty locators here...
	 */
	switch (brdtyp) {
#ifdef MVME181
	case BRD_180:
	case BRD_181:
		if (ca->ca_paddr != M181_DUART)
			return 0;
		break;
#endif
#ifdef MVME188
	case BRD_188:
		if (ca->ca_paddr != DART_BASE)
			return 0;
		break;
#endif
	default:
		return 0;
	}

	if (bus_space_map(ca->ca_iot, ca->ca_paddr, DART_SIZE << 2, 0, &ioh) !=
	    0)
		return 0;
	rc = badaddr((vaddr_t)bus_space_vaddr(ca->ca_iot, ioh), 4);
	bus_space_unmap(ca->ca_iot, ca->ca_paddr, DART_SIZE << 2);

	return rc == 0;
}

void
dartattach(struct device *parent, struct device *self, void *aux)
{
	struct dartsoftc *sc = (struct dartsoftc *)self;
	struct mc68681_softc *msc = &sc->sc_base;
	struct confargs *ca = aux;
	bus_space_handle_t ioh;

	if (ca->ca_ipl < 0)
		ca->ca_ipl = IPL_TTY;

	sc->sc_iot = ca->ca_iot;
	if (bus_space_map(sc->sc_iot, ca->ca_paddr, DART_SIZE << 2, 0, &ioh) !=
	    0) {
		printf(": can't map registers!\n");
		return;
	}
	sc->sc_ioh = ioh;

	/*
	 * Although we are still running using the BUG routines,
	 * this device will be elected as the console after autoconf.
	 * We do not even test since we know we are an MVME181 or
	 * an MVME188 and console is always on the first port.
	 */
	msc->sc_consport = CONS_PORT;
	msc->sc_sw_reg = &msc->sc_sw_reg_store;

	msc->sc_read = dart_read;
	msc->sc_write = dart_write;

	/*
	 * Interrupt configuration.
	 * When running on an AngelFire board, we need to enable the timer
	 * interrupt since the timer output is not wired to a specific
	 * output port.
	 */
#ifdef MVME181
	if (brdtyp == BRD_180 || brdtyp == BRD_181)
		msc->sc_sw_reg->imr = DART_ISR_CT;
#endif

	/*
	 * Input Port configuration.
	 *
	 * On MVME188, they are as follows:
	 *   IP0 = port A CTS
	 *   IP1 = port B CTS
	 *   IP2 and IP4 = port A DCD
	 *   IP3 and IP5 = port B DCD
	 */
	msc->sc_hw[A_PORT].dcd_ip = 2;
	msc->sc_hw[A_PORT].dcd_active_low = 1;
	msc->sc_hw[B_PORT].dcd_ip = 3;
	msc->sc_hw[B_PORT].dcd_active_low = 1;
	msc->sc_sw_reg->acr |=
	    DART_ACR_ISR_IP3_CHANGE_ENABLE | DART_ACR_ISR_IP2_CHANGE_ENABLE;

	/*
	 * Output Port configuration.
	 *
	 * On MVME188, they are as follow:
	 *   OP0 = port A RTS
	 *   OP1 = port B RTS
	 *   OP2 = port A DTR
	 *   OP3 = timer output
	 *   OP5 = port B DTR
	 *
	 * On MVME180 and MVME181 boards, the timer output is not wired to
	 * OP3. Apparently MVME180 routes port B DTR to OP3, while MVME181
	 * is MVME188 compatible.
	 */
	msc->sc_hw[A_PORT].dtr_op = 2;
	msc->sc_hw[A_PORT].rts_op = 0;
#ifdef MVME181
	if (brdtyp == BRD_180)
		msc->sc_hw[B_PORT].dtr_op = 3;
	else
#endif
		msc->sc_hw[B_PORT].dtr_op = 5;
	msc->sc_hw[B_PORT].rts_op = 1;
#ifdef MVME181
	if (brdtyp == BRD_180)
		msc->sc_sw_reg->oprs = 
		    DART_OP_OP3 | DART_OP_OP2 | DART_OP_OP1 | DART_OP_OP0;
	else
#endif
		msc->sc_sw_reg->oprs = 
		    DART_OP_OP5 | DART_OP_OP2 | DART_OP_OP1 | DART_OP_OP0;
	msc->sc_sw_reg->opcr = DART_OPCR_OP7 | DART_OPCR_OP6 |
	    DART_OPCR_OP5 | DART_OPCR_OP4 | DART_OPCR_OP3 | DART_OPCR_OP2;

	/*
	 * Clock configuration.
	 *
	 * MVME181 uses the timer mode, MVME188 uses the counter mode.
	 */
#ifdef MVME181
	if (brdtyp == BRD_180 || brdtyp == BRD_181) {
		extern int m181_clkint;	/* XXX */

		msc->sc_sw_reg->acr = DART_ACR_CT_TIMER_CLK_16;
		msc->sc_sw_reg->ct = &m181_clkint;
	} else
#endif
		msc->sc_sw_reg->acr = DART_ACR_CT_COUNTER_CLK_16;

	mc68681_common_attach(msc);

	/* enable interrupts */
	sc->sc_ih.ih_fn = dartintr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_wantframe = 0;
	sc->sc_ih.ih_ipl = ca->ca_ipl;

	platform->intsrc_establish(INTSRC_DUART, &sc->sc_ih, self->dv_xname);
}

int
dartintr(void *arg)
{
	struct mc68681_softc *sc = arg;
	uint8_t isr, imr;
	int rc;

	isr = (*sc->sc_read)(sc, DART_ISR);
	imr = sc->sc_sw_reg->imr;

	isr &= imr;
	if (isr == 0)
		return 0;

	rc = 1;

#ifdef MVME181
	if (isr & DART_ISR_CT) {
		rc = -1;	/* needs to be handled by the second
				   interrupt handler */
		isr &= ~DART_ISR_CT;
	}
#endif
	if (isr != 0)
		mc68681_intr(sc, isr);

	return rc;
}

/*
 * Console interface routines.
 * Since we select the actual console after all devices are attached,
 * we can safely pick the appropriate softc and use its information.
 */

extern struct cfdriver dart_cd;

void
dartcnprobe(struct consdev *cp)
{
	int maj;

	switch (brdtyp) {
#ifdef MVME181
	case BRD_180:
	case BRD_181:
		break;
#endif
#ifdef MVME188
	case BRD_188:
		break;
#endif
	default:
		return;
	}

	/* do not attach as console if dart0 has not attached */
	if (dart_cd.cd_ndevs == 0 || dart_cd.cd_devs[0] == NULL)
		return;

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == dartopen)
			break;
	if (maj == nchrdev)
		return;

	cp->cn_dev = makedev(maj, CONS_PORT);
	cp->cn_pri = CN_LOWPRI;
}

void
dartcninit(cp)
	struct consdev *cp;
{
}

void
dartcnputc(dev_t dev, int c)
{
	struct dartsoftc *sc;
	int s;

	sc = (struct dartsoftc *)dart_cd.cd_devs[0];

	s = spltty();

	/* inhibit interrupts on the chip */
	dart_write(sc, DART_IMR, sc->sc_base.sc_sw_reg->imr & ~DART_ISR_TXA);
	/* make sure transmitter is enabled */
	dart_write(sc, DART_CRA, DART_CR_TX_ENABLE);

	while ((dart_read(sc, DART_SRA) & DART_SR_TX_READY) == 0)
		;
	dart_write(sc, DART_TBA, c);

	/* wait for transmitter to empty */
	while ((dart_read(sc, DART_SRA) & DART_SR_TX_EMPTY) == 0)
		;

	/* restore the previous state */
	dart_write(sc, DART_IMR, sc->sc_base.sc_sw_reg->imr);
	dart_write(sc, DART_CRA, sc->sc_base.sc_sw_reg->cr[0]);

	splx(s);
}

int
dartcngetc(dev_t dev)
{
	struct dartsoftc *sc;
	uint8_t sr;		/* status reg of port a/b */
	u_char c;		/* received character */
	int s;

	sc = (struct dartsoftc *)dart_cd.cd_devs[0];

	s = spltty();

	/* enable receiver */
	dart_write(sc, DART_CRA, DART_CR_RX_ENABLE);

	for (;;) {
		/* read status reg */
		sr = dart_read(sc, DART_SRA);

		/* receiver interrupt handler */
		if (sr & DART_SR_RX_READY) {
			/* read character from port */
			c = dart_read(sc, DART_RBA);

			/* check break and error conditions */
			if (sr & DART_SR_BREAK) {
				dart_write(sc, DART_CRA, DART_CR_RESET_BREAK);
				dart_write(sc, DART_CRA, DART_CR_RESET_ERROR);
			} else if (sr & (DART_SR_FRAME | DART_SR_PARITY |
			    DART_SR_OVERRUN)) {
				dart_write(sc, DART_CRA, DART_CR_RESET_ERROR);
			} else
				break;
		}
	}
	splx(s);

	return (int)c;
}
