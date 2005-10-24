/*	$OpenBSD: comvar.h,v 1.37 2005/10/24 14:22:34 fgsch Exp $	*/
/*	$NetBSD: comvar.h,v 1.5 1996/05/05 19:50:47 christos Exp $	*/

/*
 * Copyright (c) 1997 - 1998, Jason Downs.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/timeout.h>

struct commulti_attach_args {
	int		ca_slave;		/* slave number */

	bus_space_tag_t ca_iot;
	bus_space_handle_t ca_ioh;
	int		ca_iobase;
	int		ca_noien;
};

#define	COM_IBUFSIZE	(2 * 512)
#define	COM_IHIGHWATER	((3 * COM_IBUFSIZE) / 4)

struct com_softc {
	struct device sc_dev;
	void *sc_ih;
	bus_space_tag_t sc_iot;
	struct tty *sc_tty;
	struct timeout sc_dtr_tmo;
	struct timeout sc_diag_tmo;
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
	void *sc_si;
#else
	struct timeout sc_comsoft_tmo;
#endif

	int sc_overflows;
	int sc_floods;
	int sc_errors;

	int sc_halt;

	bus_addr_t sc_iobase;
	int sc_frequency;

	bus_space_handle_t sc_ioh;

	u_char sc_uarttype;
#define COM_UART_UNKNOWN	0x00		/* unknown */
#define COM_UART_8250		0x01		/* no fifo */
#define COM_UART_16450		0x02		/* no fifo */
#define COM_UART_16550		0x03		/* no working fifo */
#define COM_UART_16550A		0x04		/* 16 byte fifo */
#define COM_UART_ST16650	0x05		/* no working fifo */
#define COM_UART_ST16650V2	0x06		/* 32 byte fifo */
#define COM_UART_TI16750	0x07		/* 64 byte fifo */
#define	COM_UART_XR16850	0x10		/* 128 byte fifo */
#define COM_UART_PXA2X0		0x11		/* 16 byte fifo */

	u_char sc_hwflags;
#define	COM_HW_NOIEN	0x01
#define	COM_HW_FIFO	0x02
#define	COM_HW_SIR	0x20
#define	COM_HW_CONSOLE	0x40
#define	COM_HW_KGDB	0x80
	u_char sc_swflags;
#define	COM_SW_SOFTCAR	0x01
#define	COM_SW_CLOCAL	0x02
#define	COM_SW_CRTSCTS	0x04
#define	COM_SW_MDMBUF	0x08
#define	COM_SW_PPS	0x10
	int	sc_fifolen;
	u_char sc_msr, sc_mcr, sc_lcr, sc_ier;
	u_char sc_dtr;

	u_char	sc_cua;

	u_char	sc_initialize;		/* force initialization */

	u_char *sc_ibuf, *sc_ibufp, *sc_ibufhigh, *sc_ibufend;
	u_char sc_ibufs[2][COM_IBUFSIZE];

	/* power management hooks */
	int (*enable)(struct com_softc *);
	void (*disable)(struct com_softc *);
	int enabled;
};

int	comprobe1(bus_space_tag_t, bus_space_handle_t);
int	comstop(struct tty *, int);
int	comintr(void *);
int	com_detach(struct device *, int);
int	com_activate(struct device *, enum devact);

void	comdiag(void *);
int	comspeed(long, long);
u_char	com_cflag2lcr(tcflag_t); /* XXX undefined */
int	comparam(struct tty *, struct termios *);
void	comstart(struct tty *);
void	comsoft(void *);

struct consdev;
int	comcnattach(bus_space_tag_t, bus_addr_t, int, int, tcflag_t);
void	comcnprobe(struct consdev *);
void	comcninit(struct consdev *);
int	comcngetc(dev_t);
void	comcnputc(dev_t, int);
void	comcnpollc(dev_t, int);
int	com_common_getc(bus_space_tag_t, bus_space_handle_t);
void	com_common_putc(bus_space_tag_t, bus_space_handle_t, int);
void	com_raisedtr(void *);

#ifdef KGDB
extern bus_addr_t com_kgdb_addr;
extern bus_space_tag_t com_kgdb_iot;
extern bus_space_handle_t com_kgdb_ioh;

int	com_kgdb_attach(bus_space_tag_t, bus_addr_t, int, int, tcflag_t);
int	kgdbintr(void *);
#endif

void com_attach_subr(struct com_softc *);

extern int comdefaultrate;
extern int comconsfreq;
extern bus_addr_t comconsaddr;
extern bus_addr_t comsiraddr;
extern int comconsinit;
extern int comconsattached;
extern bus_space_tag_t comconsiot;
extern bus_space_handle_t comconsioh;
extern tcflag_t comconscflag;
