/*	$OpenBSD: dart.c,v 1.13 2013/10/17 15:46:18 miod Exp $	*/

/*
 * Copyright (c) 2006, 2013 Miodrag Vallat.
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

#include <machine/avcommon.h>
#include <aviion/dev/sysconvar.h>
#include <dev/ic/mc68681reg.h>
#include <dev/ic/mc68681var.h>

struct dartsoftc {
	struct mc68681_softc	sc_base;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	struct intrhand		sc_ih;
};

int	dart_match(struct device *parent, void *self, void *aux);
void	dart_attach(struct device *parent, struct device *self, void *aux);

struct cfattach dart_ca = {
	sizeof(struct dartsoftc), dart_match, dart_attach
};

/* console, if applicable, will always be on the first port */
#define	CONS_PORT	A_PORT

cons_decl(dart);
int	dartintr(void *);
uint8_t	dart_read(void *, uint);
void	dart_write(void *, uint, uint8_t);

/* early console register image */
struct mc68681_sw_reg	dartcn_sw_reg;

/*
 * DUART registers are mapped as the least-significant byte of 32-bit addresses.
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
dart_match(struct device *parent, void *cf, void *aux)
{
	struct confargs *ca = aux;
	bus_space_handle_t ioh;
	int rc;

	/*
	 * We do not accept empty locators here...
	 */
	if (ca->ca_paddr == (paddr_t)-1)
		return (0);

	if (bus_space_map(ca->ca_iot, ca->ca_paddr, DART_SIZE << 2, 0, &ioh) !=
	    0)
		return (0);
	rc = badaddr((vaddr_t)bus_space_vaddr(ca->ca_iot, ioh), 4);
	bus_space_unmap(ca->ca_iot, ca->ca_paddr, DART_SIZE << 2);

	return (rc == 0);
}

void
dart_attach(struct device *parent, struct device *self, void *aux)
{
	struct dartsoftc *sc = (struct dartsoftc *)self;
	struct mc68681_softc *msc = &sc->sc_base;
	struct confargs *ca = aux;
	bus_space_handle_t ioh;
	u_int intsrc;

	sc->sc_iot = ca->ca_iot;
	if (bus_space_map(sc->sc_iot, ca->ca_paddr, DART_SIZE << 2, 0, &ioh) !=
	    0) {
		printf(": can't map registers!\n");
		return;
	}
	sc->sc_ioh = ioh;

	if (ca->ca_paddr == CONSOLE_DART_BASE) {
		intsrc = INTSRC_DUART1;
		msc->sc_consport = CONS_PORT;	/* XXX always for now */
		msc->sc_sw_reg = &dartcn_sw_reg;
	} else {
		intsrc = INTSRC_DUART2;
		msc->sc_sw_reg = &msc->sc_sw_reg_store;
		msc->sc_consport = -1;
	}

	msc->sc_read = dart_read;
	msc->sc_write = dart_write;

	/*
	 * Interrupt configuration.
	 *
	 * Timer interrupts are not routed anywhere, so we don't need
	 * to enable them.
	 */
	msc->sc_sw_reg->imr = 0;

	/*
	 * Input Port configuration.
	 *
	 * There is no documentation about the serial port usage
	 * of the input port in the 400 and 4600 manuals. However,
	 * the 6280 manual describes canonical assignments, which
	 * are hopefully correct on other systems:
	 *   IP0 = port A CTS
	 *   IP1 = port B CTS
	 *   IP2 = port A DCD (active low)
	 *   IP3 = port B DCD (active low)
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
	 * Again, the serial bits assigments are not documented until the
	 * 6280 manual, except for the speaker connected to OP3.
	 *   OP0 = port A RTS (guessed)
	 *   OP1 = port B RTS (guessed)
	 *   OP2 = port A DTR
	 *   OP3 = speaker input or port B DTR
	 *   OP4 = speaker enable (active low)
	 *   OP5 = port B DTR if speaker on OP3
	 *   OP6 = parallel port data strobe
	 *   OP7 = parallel port selection
	 */
	msc->sc_sw_reg->opcr = DART_OPCR_OP7 | DART_OPCR_OP6 |
	    DART_OPCR_OP5 | DART_OPCR_OP4 | DART_OPCR_OP3 | DART_OPCR_OP2;
	msc->sc_hw[A_PORT].dtr_op = 2;
	msc->sc_hw[A_PORT].rts_op = 0;
	switch (cpuid) {
	case AVIION_300_310:
	case AVIION_400_4000:
	case AVIION_410_4100:
	case AVIION_300C_310C:
	case AVIION_300CD_310CD:
	case AVIION_300D_310D:
	case AVIION_4300_25:
	case AVIION_4300_20:
	case AVIION_4300_16:
		msc->sc_sw_reg->oprs |= DART_OP_OP4;	/* disable speaker */
		/* FALLTHROUGH */
	case AVIION_4600_530:
		msc->sc_hw[B_PORT].dtr_op = 5;
		msc->sc_sw_reg->oprs |= DART_OP_OP5;
		break;
	default:
		msc->sc_hw[B_PORT].dtr_op = 3;
		msc->sc_sw_reg->oprs |= DART_OP_OP3;
		break;
	}
	msc->sc_hw[B_PORT].rts_op = 1;
	msc->sc_sw_reg->oprs |= DART_OP_OP2 | DART_OP_OP1 | DART_OP_OP0;

	/*
	 * Clock configuration.
	 *
	 * We don't use any clock; the MI driver expects a counter mode
	 * to be set in this case.
	 */
	msc->sc_sw_reg->acr = DART_ACR_CT_COUNTER_CLK_16;

	mc68681_common_attach(msc);

	/* enable interrupts */
	sc->sc_ih.ih_fn = dartintr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_flags = 0;
	sc->sc_ih.ih_ipl = IPL_TTY;

	sysconintr_establish(intsrc, &sc->sc_ih, self->dv_xname);
}

int
dartintr(void *arg)
{
	struct mc68681_softc *sc = arg;
	uint8_t isr, imr;

	isr = (*sc->sc_read)(sc, DART_ISR);
	imr = sc->sc_sw_reg->imr;

	isr &= imr;
	if (isr == 0)
		return 0;

	mc68681_intr(sc, isr);
	return 1;
}

/*
 * Console interface routines.
 */

#define	dart_cnread(reg) \
	*(volatile u_int32_t *)(CONSOLE_DART_BASE + ((reg) << 2))
#define	dart_cnwrite(reg, val) \
	*(volatile u_int32_t *)(CONSOLE_DART_BASE + ((reg) << 2)) = (val)

void
dartcnprobe(struct consdev *cp)
{
	int maj;

	if (badaddr(CONSOLE_DART_BASE, 4) != 0)
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
	dartcn_sw_reg.mr1[CONS_PORT] =
	    DART_MR1_RX_IRQ_RXRDY | DART_MR1_ERROR_CHAR |
	    DART_MR1_PARITY_NONE | DART_MR1_RX_RTR | DART_MR1_BPC_8;
	dartcn_sw_reg.mr2[CONS_PORT] =
	    DART_MR2_MODE_NORMAL | /* DART_MR2_TX_CTS | */ DART_MR2_STOP_1;
	dartcn_sw_reg.cr[CONS_PORT]  =
	    DART_CR_TX_ENABLE | DART_CR_RX_ENABLE;

	dartcn_sw_reg.acr = DART_ACR_BRG_SET_2 | DART_ACR_CT_COUNTER_CLK_16;
	dartcn_sw_reg.oprs = CONS_PORT == A_PORT ? DART_OP_OP2 | DART_OP_OP0 :
	    DART_OP_OP5 | DART_OP_OP3 | DART_OP_OP1;
	dartcn_sw_reg.opcr = DART_OPCR_RX_A;	/* XXX unconditional */
	dartcn_sw_reg.imr = DART_ISR_IP_CHANGE;

	dart_cnwrite(DART_CRA,
	    DART_CR_RESET_RX | DART_CR_TX_DISABLE | DART_CR_RX_DISABLE);
	DELAY(2);
	dart_cnwrite(DART_CRA,
	    DART_CR_RESET_TX /* | DART_CR_TX_DISABLE | DART_CR_RX_DISABLE */);
	DELAY(2);
	dart_cnwrite(DART_CRA,
	    DART_CR_RESET_ERROR /* | DART_CR_TX_DISABLE | DART_CR_RX_DISABLE */);
	DELAY(2);
	dart_cnwrite(DART_CRA,
	    DART_CR_RESET_BREAK /* | DART_CR_TX_DISABLE | DART_CR_RX_DISABLE */);
	DELAY(2);
	dart_cnwrite(DART_CRA,
	    DART_CR_RESET_MR1 /* | DART_CR_TX_DISABLE | DART_CR_RX_DISABLE */);
	DELAY(2);

	dart_cnwrite(DART_OPRS, dartcn_sw_reg.oprs);
	dart_cnwrite(DART_OPCR, dartcn_sw_reg.opcr);

	dart_cnwrite(DART_ACR, dartcn_sw_reg.acr);

	dart_cnwrite(DART_MRA, dartcn_sw_reg.mr1[CONS_PORT]);
	dart_cnwrite(DART_MRA, dartcn_sw_reg.mr2[CONS_PORT]);
	dart_cnwrite(DART_CSRA,
	    (DART_CSR_9600 << DART_CSR_RXCLOCK_SHIFT) |
	    (DART_CSR_9600 << DART_CSR_TXCLOCK_SHIFT));
	dart_cnwrite(DART_CRA, dartcn_sw_reg.cr[CONS_PORT]);
	DELAY(2);

	dart_cnwrite(DART_IMR, dartcn_sw_reg.imr);
}

void
dartcnputc(dev_t dev, int c)
{
	int s;

	s = spltty();

	/* inhibit interrupts on the chip */
	dart_cnwrite(DART_IMR, dartcn_sw_reg.imr & ~DART_ISR_TXA);
	/* make sure transmitter is enabled */
	dart_cnwrite(DART_CRA, DART_CR_TX_ENABLE);
	DELAY(2);

	while ((dart_cnread(DART_SRA) & DART_SR_TX_READY) == 0)
		;
	dart_cnwrite(DART_TBA, c);

	/* wait for transmitter to empty */
	while ((dart_cnread(DART_SRA) & DART_SR_TX_EMPTY) == 0)
		;

	/* restore the previous state */
	dart_cnwrite(DART_IMR, dartcn_sw_reg.imr);
	dart_cnwrite(DART_CRA, dartcn_sw_reg.cr[0]);
	DELAY(2);

	splx(s);
}

int
dartcngetc(dev_t dev)
{
	uint8_t sr;		/* status reg of port a/b */
	u_char c;		/* received character */
	int s;

	s = spltty();

	/* enable receiver */
	dart_cnwrite(DART_CRA, DART_CR_RX_ENABLE);
	DELAY(2);

	for (;;) {
		/* read status reg */
		sr = dart_cnread(DART_SRA);

		/* receiver interrupt handler*/
		if (sr & DART_SR_RX_READY) {
			/* read character from port */
			c = dart_cnread(DART_RBA);

			/* check break and error conditions */
			if (sr & DART_SR_BREAK) {
				dart_cnwrite(DART_CRA, DART_CR_RESET_BREAK);
				DELAY(2);
				dart_cnwrite(DART_CRA, DART_CR_RESET_ERROR);
				DELAY(2);
			} else if (sr & (DART_SR_FRAME | DART_SR_PARITY |
			    DART_SR_OVERRUN)) {
				dart_cnwrite(DART_CRA, DART_CR_RESET_ERROR);
				DELAY(2);
			} else
				break;
		}
	}
	splx(s);

	return (int)c;
}
