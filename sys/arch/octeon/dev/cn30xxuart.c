/*	$OpenBSD: cn30xxuart.c,v 1.1 2011/05/08 13:39:30 syuu Exp $	*/

/*
 * Copyright (c) 2001-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <machine/autoconf.h>
#include <machine/bus.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#include <dev/cons.h>

#include <octeon/dev/iobusvar.h>
#include <octeon/dev/uartbusvar.h>
#include <octeon/dev/octeonreg.h>

int	cn30xxuart_probe(struct device *, void *, void *);
void	cn30xxuart_attach(struct device *, struct device *, void *);

struct cfattach cn30xxuart_ca = {
	sizeof(struct com_softc), cn30xxuart_probe, cn30xxuart_attach
};

extern struct cfdriver com_cd;

cons_decl(cn30xxuart);

#define  OCTEON_MIO_UART0               0x8001180000000800ull
#define  OCTEON_MIO_UART0_LSR           0x8001180000000828ull
#define  OCTEON_MIO_UART0_RBR           0x8001180000000800ull
#define  OCTEON_MIO_UART0_USR           0x8001180000000938ull
#define  OCTEON_MIO_UART0_LCR           0x8001180000000818ull
#define  OCTEON_MIO_UART0_DLL           0x8001180000000880ull
#define  OCTEON_MIO_UART0_DLH           0x8001180000000888ull
#define  USR_TXFIFO_NOTFULL		2

static int delay_changed = 1;
int cn30xxuart_delay(void);
void cn30xxuart_wait_txhr_empty(int);

int
cn30xxuart_probe(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;
	struct uartbus_attach_args *uba = aux;
	bus_space_tag_t iot = uba->uba_memt;
	bus_space_handle_t ioh;
	int rv = 0, console;

	if (strcmp(uba->uba_name, com_cd.cd_name) != 0)
		return 0;

	console = 1;

	/* if it's in use as console, it's there. */
	if (!(console && !comconsattached)) {
		if (bus_space_map(iot, uba->uba_baseaddr, COM_NPORTS, 0, &ioh)) {
			printf(": can't map uart registers\n");
			return 1;
		}
		rv = comprobe1(iot, ioh);
	} else
		rv = 1;

	/* make a config stanza with exact locators match over a generic line */
	if (cf->cf_loc[0] != -1)
		rv += rv;

	return rv;
}

void
cn30xxuart_attach(struct device *parent, struct device *self, void *aux)
{
	struct com_softc *sc = (void *)self;
	struct uartbus_attach_args *uba = aux;
	int console;

	console = 1;

	sc->sc_iot = uba->uba_memt;
	sc->sc_iobase = uba->uba_baseaddr;
	sc->sc_hwflags = 0;
	sc->sc_swflags = 0;
	sc->sc_frequency = curcpu()->ci_hw.clock;
	sc->sc_uarttype = COM_UART_16550;

	/* if it's in use as console, it's there. */
	if (bus_space_map(sc->sc_iot, sc->sc_iobase, COM_NPORTS, 0, &sc->sc_ioh)) {
		printf(": can't map uart registers\n");
		return;
	}

	if (console && !comconsattached) {
		/*
		 * If we are the console, reuse the existing bus_space
		 * information, so that comcnattach() invokes bus_space_map()
		 * with correct parameters.
		 */

		if (comcnattach(sc->sc_iot, sc->sc_iobase, 115200,
		    sc->sc_frequency, (TTYDEF_CFLAG & ~(CSIZE | PARENB)) | CS8))
			panic("can't setup serial console");
	}

	com_attach_subr(sc);

	octeon_intr_establish(uba->uba_intr, IPL_TTY, comintr,
	    (void *)sc, sc->sc_dev.dv_xname);
}

/*
 * Early console routines.
 */
int 
cn30xxuart_delay(void)
{
	int divisor;
	u_char lcr;
        static int d = 0;

        if (!delay_changed) return d;
        delay_changed = 0;
	lcr = (u_char)*(uint64_t*)OCTEON_MIO_UART0_LCR;
	*(uint64_t*)OCTEON_MIO_UART0_LCR = lcr | LCR_DLAB;
	divisor = (int)(*(uint64_t*)OCTEON_MIO_UART0_DLL | 
		*(uint64_t*)OCTEON_MIO_UART0_DLH << 8);
	*(uint64_t*)OCTEON_MIO_UART0_LCR = lcr;
	
	return 10; /* return an approx delay value */
}

void
cn30xxuart_wait_txhr_empty(int d)
{
	while (((*(uint64_t*)OCTEON_MIO_UART0_LSR & LSR_TXRDY) == 0) &&
        	((*(uint64_t*)OCTEON_MIO_UART0_USR & USR_TXFIFO_NOTFULL) == 0))
		delay(d);
}

void
cn30xxuartcninit(struct consdev *consdev)
{
}

void
cn30xxuartcnprobe(struct consdev *consdev)
{
}

void
cn30xxuartcnpollc(dev_t dev, int c)
{
}

void
cn30xxuartcnputc (dev_t dev, int c)
{
	int d;

	/* 1/10th the time to transmit 1 character (estimate). */
	d = cn30xxuart_delay();
        cn30xxuart_wait_txhr_empty(d);
	*(uint64_t*)OCTEON_MIO_UART0_RBR = (uint8_t)c;
        cn30xxuart_wait_txhr_empty(d);
}

int
cn30xxuartcngetc (dev_t dev)
{
	int c, d;

	/* 1/10th the time to transmit 1 character (estimate). */
	d = cn30xxuart_delay();

	while ((*(uint64_t*)OCTEON_MIO_UART0_LSR & LSR_RXRDY) == 0)
		delay(d);

	c = (uint8_t)*(uint64_t*)OCTEON_MIO_UART0_RBR;

	return (c);
}
