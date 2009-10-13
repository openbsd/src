/*	$OpenBSD: sscom_var.h,v 1.2 2009/10/13 19:33:16 pirofti Exp $ */
/* $NetBSD: sscom_var.h,v 1.7 2006/03/06 20:21:25 rjs Exp $ */

/*
 * Copyright (c) 2002, 2003 Fujitsu Component Limited
 * Copyright (c) 2002, 2003 Genetec Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Fujitsu Component Limited nor the name of
 *    Genetec corporation may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY FUJITSU COMPONENT LIMITED AND GENETEC
 * CORPORATION ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL FUJITSU COMPONENT LIMITED OR GENETEC
 * CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* derived from sys/dev/ic/comvar.h */

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

#ifndef _ARM_S3C2XX0_SSCOM_VAR_H
#define _ARM_S3C2XX0_SSCOM_VAR_H

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/timeout.h>
#include <machine/bus.h>

#ifdef	SSCOM_S3C2410
#include <arm/s3c2xx0/s3c2410reg.h>
#include <arm/s3c2xx0/s3c2410var.h>
#endif

/* Hardware flag masks */
#define	SSCOM_HW_FLOW		0x02
#define	SSCOM_HW_DEV_OK		0x04
#define	SSCOM_HW_CONSOLE	0x08
#define	SSCOM_HW_KGDB		0x10
#define SSCOM_HW_TXINT		0x20
#define SSCOM_HW_RXINT		0x40

/* Buffer size for character buffer */
#define	SSCOM_RING_SIZE	2048

struct sscom_softc {
	struct device sc_dev;
	void *sc_si;
	struct tty *sc_tty;

	struct timeout sc_diag_timeout;

	int sc_unit;			/* UART0/UART1 */
	int sc_frequency;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	u_int sc_overflows,
	      sc_floods,
	      sc_errors;

	int sc_hwflags,
	    sc_swflags;

	u_int sc_r_hiwat,
	      sc_r_lowat;
	u_char *volatile sc_rbget,
	       *volatile sc_rbput;
 	volatile u_int sc_rbavail;
	u_char *sc_rbuf,
	       *sc_ebuf;

 	u_char *sc_tba;
 	u_int sc_tbc,
	      sc_heldtbc;

	volatile u_char sc_rx_flags,
#define	RX_TTY_BLOCKED		0x01
#define	RX_TTY_OVERFLOWED	0x02
#define	RX_IBUF_BLOCKED		0x04
#define	RX_IBUF_OVERFLOWED	0x08
#define	RX_ANY_BLOCK		0x0f
			sc_tx_busy,
			sc_tx_done,
			sc_tx_stopped,
			sc_st_check,
			sc_rx_ready;

	/* data to stored in UART registers.
	   actual write to UART register is pended while sc_tx_busy */
	uint16_t sc_ucon;		/* control register */
	uint16_t sc_ubrdiv;		/* baudrate register */
	uint8_t  sc_heldchange;		/* register changes are pended */
	uint8_t  sc_ulcon;		/* line control */
	uint8_t  sc_umcon;		/* modem control */
#define  UMCON_HW_MASK	(UMCON_RTS)
#define  UMCON_DTR  (1<<4)		/* provided by other means such as GPIO */
	uint8_t  sc_msts;		/* modem status */
#define  MSTS_CTS   UMSTAT_CTS		/* bit0 */
#define  MSTS_DCD   (1<<1)
#define  MSTS_DSR   (1<<2)

	uint8_t sc_msr_dcd;		/* DCD or 0 */
	uint8_t sc_mcr_dtr;		/* DTR or 0 or DTR|RTS*/
	uint8_t sc_mcr_rts;		/* RTS or DTR in sc_umcon */
	uint8_t sc_msr_cts;		/* CTS or DCD in sc_msts */

	uint8_t sc_msr_mask;		/* sc_msr_cts|sc_msr_dcd */
	uint8_t sc_mcr_active;
	uint8_t sc_msr_delta;

	uint8_t sc_rx_irqno, sc_tx_irqno;

#if 0
	/* PPS signal on DCD, with or without inkernel clock disciplining */
	u_char	sc_ppsmask;			/* pps signal mask */
	u_char	sc_ppsassert;			/* pps leading edge */
	u_char	sc_ppsclear;			/* pps trailing edge */
	pps_info_t ppsinfo;
	pps_params_t ppsparam;
#endif

#if NRND > 0 && defined(RND_COM)
	rndsource_element_t  rnd_source;
#endif
#if (defined(MULTIPROCESSOR) || defined(LOCKDEBUG)) && defined(SSCOM_MPLOCK)
	struct simplelock	sc_lock;
#endif

	/*
	 * S3C2XX0's UART doesn't have modem control/status pins.
	 * On platforms with S3C2XX0, those pins are simply unavailable
	 * or provided by other means such as GPIO.  Platform specific attach routine
	 * have to provide functions to read/write modem control/status pins.
	 */
	int	(* read_modem_status)( struct sscom_softc * );
	void	(* set_modem_control)( struct sscom_softc * );
	int sc_cua;
};

/* UART register address, etc. */
struct sscom_uart_info {
	int		unit;
	char		tx_int, rx_int, err_int;
	bus_addr_t	iobase;
};

#define sscom_rxrdy(iot,ioh) \
	(bus_space_read_1((iot), (ioh), SSCOM_UTRSTAT) & UTRSTAT_RXREADY)
#define sscom_getc(iot,ioh) bus_space_read_1((iot), (ioh), SSCOM_URXH)
#define sscom_geterr(iot,ioh) bus_space_read_1((iot), (ioh), SSCOM_UERSTAT)

/* 
 * we need to tweak interrupt controller to mask/unmask rxint and/or txint.
 */
#ifdef SSCOM_S3C2410
/* RXINTn, TXINTn and ERRn interrupts are cascaded to UARTn irq. */

#define	_sscom_intbit(irqno)	(1<<((irqno)-S3C2410_SUBIRQ_MIN))

#define	sscom_unmask_rxint(sc)	\
	s3c2410_unmask_subinterrupts(_sscom_intbit((sc)->sc_rx_irqno))
#define	sscom_mask_rxint(sc)	\
	s3c2410_mask_subinterrupts(_sscom_intbit((sc)->sc_rx_irqno))
#define	sscom_unmask_txint(sc)	\
	s3c2410_unmask_subinterrupts(_sscom_intbit((sc)->sc_tx_irqno))
#define	sscom_mask_txint(sc)	\
	s3c2410_mask_subinterrupts(_sscom_intbit((sc)->sc_tx_irqno))
#define	sscom_unmask_txrxint(sc)                                      \
	s3c2410_unmask_subinterrupts(_sscom_intbit((sc)->sc_tx_irqno) | \
			             _sscom_intbit((sc)->sc_rx_irqno))
#define	sscom_mask_txrxint(sc)	                                    \
	s3c2410_mask_subinterrupts(_sscom_intbit((sc)->sc_tx_irqno) | \
			           _sscom_intbit((sc)->sc_rx_irqno))

int	sscom_cnattach(bus_space_tag_t, const struct sscom_uart_info *, 
	    int, int, tcflag_t);
int s3c2410_sscom_cnattach(bus_space_tag_t iot, int unit, int rate,
    int frequency, tcflag_t cflag);
#else

/* for S3C2800 and S3C2400 */
#define	sscom_unmask_rxint(sc)	s3c2xx0_unmask_interrupts(1<<(sc)->sc_rx_irqno)
#define	sscom_mask_rxint(sc)	s3c2xx0_mask_interrupts(1<<(sc)->sc_rx_irqno)
#define	sscom_unmask_txint(sc)	s3c2xx0_unmask_interrupts(1<<(sc)->sc_tx_irqno)
#define	sscom_mask_txint(sc)	s3c2xx0_mask_interrupts(1<<(sc)->sc_tx_irqno)
#define	sscom_unmask_txrxint(sc) \
	s3c2xx0_unmask_interrupts((1<<(sc)->sc_tx_irqno)|(1<<(sc)->sc_rx_irqno))
#define	sscom_mask_txrxint(sc)	\
	s3c2xx0_mask_interrupts((1<<(sc)->sc_tx_irqno)|(1<<(sc)->sc_rx_irqno))

#endif /* SSCOM_S3C2410 */

#define sscom_enable_rxint(sc)		\
	(sscom_unmask_rxint(sc), ((sc)->sc_hwflags |= SSCOM_HW_RXINT))
#define sscom_disable_rxint(sc)		\
	(sscom_mask_rxint(sc), ((sc)->sc_hwflags &= ~SSCOM_HW_RXINT))
#define sscom_enable_txint(sc)		\
	(sscom_unmask_txint(sc), ((sc)->sc_hwflags |= SSCOM_HW_TXINT))
#define sscom_disable_txint(sc)		\
	(sscom_mask_txint(sc),((sc)->sc_hwflags &= ~SSCOM_HW_TXINT))
#define sscom_enable_txrxint(sc) 	\
	(sscom_unmask_txrxint(sc),((sc)->sc_hwflags |= (SSCOM_HW_TXINT|SSCOM_HW_RXINT)))
#define sscom_disable_txrxint(sc)	\
	(sscom_mask_txrxint(sc),((sc)->sc_hwflags &= ~(SSCOM_HW_TXINT|SSCOM_HW_RXINT)))


int	sscomspeed(long, long);
void	sscom_attach_subr(struct sscom_softc *);

int	sscom_detach(struct device *, int);
int	sscom_activate(struct device *, int);
void	sscom_shutdown(struct sscom_softc *);
void	sscomdiag		(void *);
void	sscomstart(struct tty *);
int	sscomparam(struct tty *, struct termios *);
int	sscomread(dev_t, struct uio *, int);
void	sscom_config(struct sscom_softc *);

int	sscomtxintr(void *);
int	sscomrxintr(void *);

int	sscom_cnattach(bus_space_tag_t, const struct sscom_uart_info *, 
	    int, int, tcflag_t);
void	sscom_cndetach(void);
int	sscom_is_console(bus_space_tag_t, int, bus_space_handle_t *);


#ifdef KGDB
int	sscom_kgdb_attach(bus_space_tag_t, const struct sscom_uart_info *,
	    int, int, tcflag_t);
#endif

#endif /* _ARM_S3C2XX0_SSCOM_VAR_H */
