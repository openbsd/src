/*	$OpenBSD: uart.c,v 1.8 2014/07/12 14:15:06 jasper Exp $	*/

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
#include <sys/device.h>
#include <sys/tty.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#include <dev/cons.h>
#include <machine/octeonvar.h>

#include "libsa.h"

#define  OCTEON_MIO_UART0_RBR           0x0001180000000800ull
#define  OCTEON_MIO_UART0_IER           0x0001180000000808ull
#define  OCTEON_MIO_UART0_IIR           0x0001180000000810ull
#define  OCTEON_MIO_UART0_LCR           0x0001180000000818ull
#define  OCTEON_MIO_UART0_MCR           0x0001180000000820ull
#define  OCTEON_MIO_UART0_LSR           0x0001180000000828ull
#define  OCTEON_MIO_UART0_MSR           0x0001180000000830ull
#define  OCTEON_MIO_UART0_THR           0x0001180000000840ull
#define  OCTEON_MIO_UART0_USR           0x0001180000000938ull
#define  OCTEON_MIO_UART0_FCR           0x0001180000000850ull
#define  OCTEON_MIO_UART0_DLL           0x0001180000000880ull
#define  OCTEON_MIO_UART0_DLH           0x0001180000000888ull

#define  USR_TXFIFO_NOTFULL		2

int cn30xxuart_delay(void);
void cn30xxuart_wait_txhr_empty(int);

/*
 * Early console routines.
 */
int
cn30xxuart_delay(void)
{
	int divisor;
	int lcr;
	lcr = octeon_xkphys_read_8(OCTEON_MIO_UART0_LCR);
	octeon_xkphys_write_8(OCTEON_MIO_UART0_LCR, lcr | LCR_DLAB);

	divisor = (octeon_xkphys_read_8(OCTEON_MIO_UART0_DLL) |
			    (octeon_xkphys_read_8(OCTEON_MIO_UART0_DLH) << 8));
	octeon_xkphys_write_8(OCTEON_MIO_UART0_LCR, lcr);

	return (10);
}

void
cn30xxuart_wait_txhr_empty(int d)
{
	while(((octeon_xkphys_read_8(OCTEON_MIO_UART0_LSR) & LSR_TXRDY) == 0) &&
	        ((octeon_xkphys_read_8(OCTEON_MIO_UART0_USR) & USR_TXFIFO_NOTFULL) == 0))
		delay(d);
}

void
cn30xxuartcninit(struct consdev *consdev)
{
	int ier;
	/* Disable interrupts */
	ier = octeon_xkphys_read_8(OCTEON_MIO_UART0_IER) & 0x0;
	octeon_xkphys_write_8(OCTEON_MIO_UART0_IER, ier);

	/* Enable RTS & DTR */
	octeon_xkphys_write_8(OCTEON_MIO_UART0_MCR, MCR_RTS | MCR_DTR);
}

void
cn30xxuartcnprobe(struct consdev *cn)
{
	cn->cn_pri = CN_HIGHPRI;
	cn->cn_dev = makedev(CONSMAJOR, 0);
}

void
cn30xxuartcnputc (dev_t dev, int c)
{
	int d;

	/* 1/10th the time to transmit 1 character (estimate). */
	d = cn30xxuart_delay();
        cn30xxuart_wait_txhr_empty(d);
	octeon_xkphys_write_8(OCTEON_MIO_UART0_THR, c);
        cn30xxuart_wait_txhr_empty(d);
}

int
cn30xxuartcngetc (dev_t dev)
{
	int c, d;

	/* 1/10th the time to transmit 1 character (estimate). */
	d = cn30xxuart_delay();

	if (dev & 0x80)
		return octeon_xkphys_read_8(OCTEON_MIO_UART0_LSR) & LSR_RXRDY;

	while ((octeon_xkphys_read_8(OCTEON_MIO_UART0_LSR) & LSR_RXRDY) == 0)
		delay(d);

	c = octeon_xkphys_read_8(OCTEON_MIO_UART0_RBR);
	if (c == '\r')
		c = '\n';

	return (c);
}
