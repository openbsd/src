/*	$OpenBSD: comvar.h,v 1.16 2000/02/04 06:11:58 angelos Exp $	*/
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Jason Downs for the
 *      OpenBSD system.
 * 4. Neither the name(s) of the author(s) nor the name OpenBSD
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

	int sc_overflows;
	int sc_floods;
	int sc_errors;

	int sc_halt;

	int sc_iobase;
#ifdef COM_HAYESP
	int sc_hayespbase;
#endif

	bus_space_handle_t sc_ioh;
	bus_space_handle_t sc_hayespioh;

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

	u_char sc_hwflags;
#define	COM_HW_NOIEN	0x01
#define	COM_HW_FIFO	0x02
#define	COM_HW_HAYESP	0x04
#define	COM_HW_CONSOLE	0x40
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
	int (*enable) __P((struct com_softc *));
	void (*disable) __P((struct com_softc *));
	int enabled;
};

int	comprobe1 __P((bus_space_tag_t, bus_space_handle_t));
void	cominit __P((bus_space_tag_t, bus_space_handle_t, int));
int	comstop __P((struct tty *, int));
int	comintr __P((void *));
int	com_detach __P((struct device *, int));
int	com_activate __P((struct device *, enum devact));

#ifdef COM_HAYESP
int comprobeHAYESP __P((bus_space_handle_t hayespioh, struct com_softc *sc));
#endif
void	comdiag		__P((void *));
int	comspeed	__P((long));
int	comparam	__P((struct tty *, struct termios *));
void	comstart	__P((struct tty *));
void	compoll		__P((void *));

struct consdev;
void	comcnprobe	__P((struct consdev *));
void	comcninit	__P((struct consdev *));
int	comcngetc	__P((dev_t));
void	comcnputc	__P((dev_t, int));
void	comcnpollc	__P((dev_t, int));

extern int comdefaultrate;
extern int comconsaddr;
extern int comconsinit;
extern int comconsattached;
extern bus_space_tag_t comconsiot;
extern bus_space_handle_t comconsioh;
extern tcflag_t comconscflag;
