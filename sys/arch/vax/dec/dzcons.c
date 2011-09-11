/*	$OpenBSD: dzcons.c,v 1.5 2011/09/11 19:29:01 miod Exp $	*/
/*	$NetBSD: dz_ibus.c,v 1.15 1999/08/27 17:50:42 ragge Exp $ */
/*
 * Copyright (c) 1998 Ludd, University of Lule}, Sweden.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *     This product includes software developed at Ludd, University of 
 *     Lule}, Sweden and its contributors.
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

/*
 * Console routines for system using DZ11-like controllers, with the
 * same register mapping as found on VAXstations.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <dev/cons.h>

#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/vsbus.h>
#include <machine/ka420.h>
#ifdef VAX60
#include <vax/mbus/mbusreg.h>
#include <vax/mbus/mbusvar.h>
#include <vax/mbus/fwioreg.h>
#endif

#include <vax/qbus/dzreg.h>
#include <vax/qbus/dzvar.h>

#include <vax/dec/dzkbdvar.h>

vaddr_t dz_console_regs; /* console registers mapping */

#define REG(name)     short name; short X##name##X;
static volatile struct ss_dz {/* base address of DZ-controller: 0x200a0000 */
	REG(csr);	/* 00 Csr: control/status register */
	REG(rbuf);	/* 04 Rbuf/Lpr: receive buffer/line param reg. */
	REG(tcr);	/* 08 Tcr: transmit console register */
	REG(tdr);	/* 0C Msr/Tdr: modem status reg/transmit data reg */
	REG(lpr0);	/* 10 Lpr0: */
	REG(lpr1);	/* 14 Lpr0: */
	REG(lpr2);	/* 18 Lpr0: */
	REG(lpr3);	/* 1C Lpr0: */
} *dz;
#undef REG

cdev_decl(dz);
cons_decl(dz);

extern int getmajor(void *);	/* conf.c */

int	dzcngetc_internal(int);

/*
 * Receive a character on the given line (blocking call).
 * Used by both serial and keyboard cngetc routines.
 */
int
dzcngetc_internal(int line)
{
	u_short rbuf;

	for (;;) {
		while ((dz->csr & DZ_CSR_RX_DONE) == 0)
			; /* Wait for char */
		rbuf = dz->rbuf;
		if (((rbuf >> 8) & 3) != line)
			continue;
		return (rbuf & 0xff);
	}
}

/*
 * Returns whether the dz attachment may have a keyboard plugged in.
 * There are no specific parameters identifying the dz chip, since
 * - VAXstation can only have one non-qbus dz chip.
 * - ...except for the KA60 (VAXstation 3500), which can have keyboards
 *   connected to all I/O modules, if more than one.
 * - ...and QBus dz attachments do not invoke this function as they know
 *   they are ``regular'' serial lines only.
 */
int
dz_can_have_kbd()
{
	switch (vax_boardtype) {
	case VAX_BTYP_410:
	case VAX_BTYP_420:
	case VAX_BTYP_43:
		if ((vax_confdata & KA420_CFG_MULTU) == 0)
			return (1);
		break;

	case VAX_BTYP_46:
		if ((vax_siedata & 0xff) == VAX_VTYP_46)
			return (1);
		break;
	case VAX_BTYP_48:
		if (((vax_siedata >> 8) & 0xff) == VAX_STYP_48)
			return (1);
		break;

	case VAX_BTYP_49:
#ifdef VAX60
	case VAX_BTYP_60:
#endif
		return (1);

	default:
		break;
	}

	return (0);
}

int
dzcngetc(dev) 
	dev_t dev;
{
	int c = 0, s;
	int line = minor(dev);

	s = spltty();
	do {
		c = dzcngetc_internal(line);
	} while (c == 17 || c == 19);		/* ignore XON/XOFF */
	splx(s);

	if (c == 13)
		c = 10;

	return (c);
}

void
dzcnprobe(cndev)
	struct	consdev *cndev;
{
	extern	vaddr_t iospace;
	int diagcons, major, pri;
	paddr_t ioaddr = DZ_CSR;

	if ((major = getmajor(dzopen)) < 0)
		return;

	pri = CN_DEAD;

	switch (vax_boardtype) {
	case VAX_BTYP_410:
	case VAX_BTYP_420:
	case VAX_BTYP_43:
		diagcons = (vax_confdata & KA420_CFG_L3CON ? 3 : 0);
		break;

	case VAX_BTYP_46:
	case VAX_BTYP_48:
		diagcons = (vax_confdata & 0x100 ? 3 : 0);
		break;

	case VAX_BTYP_49:
		ioaddr = DZ_CSR_KA49;
		diagcons = (vax_confdata & 8 ? 3 : 0);
		break;

	case VAX_BTYP_1303:
		ioaddr = DZ_CSR_KA49;
		diagcons = 3;
		break;

#ifdef VAX60
	case VAX_BTYP_60:
		ioaddr = MBUS_SLOT_BASE(mbus_ioslot) + FWIO_DZ_REG_OFFSET;
		diagcons = 3;
		pri = CN_LOWPRI;	/* graphics console always wins */
		break;
#endif

	default:
		return;
	}

	if (pri == CN_DEAD)
		pri = diagcons != 0 ? CN_HIGHPRI : CN_LOWPRI;
	cndev->cn_pri = pri;
	cndev->cn_dev = makedev(major, dz_can_have_kbd() ? 3 : diagcons);
	dz_console_regs = iospace;
	ioaccess(iospace, ioaddr, 1);
}

void
dzcninit(cndev)
	struct	consdev *cndev;
{
	dzcninit_internal(minor(cndev->cn_dev), 0);
}

void
dzcninit_internal(int line, int iskbd)
{
	int speed;

	dz = (void *)dz_console_regs;

	speed = iskbd ? DZ_LPR_B4800 : DZ_LPR_B9600;

	dz->csr = 0;		/* Disable scanning until initting is done */
	dz->tcr = 1 << line;	/* Turn on xmitter */
	dz->csr = DZ_CSR_MSE;	/* Turn scanning back on */
	dz->rbuf = DZ_LPR_RX_ENABLE | (speed << 8) | DZ_LPR_8_BIT_CHAR | line;
}

/*
 * IMPORTANT! Do not use major(dev) in dzcnputc(), as dzputc() only provides
 * meaningful minor when invoking dzcnputc().
 */
void
dzcnputc(dev,ch)
	dev_t	dev;
	int	ch;
{
	int timeout = 1<<15;       /* don't hang the machine! */
	int s;
	int mino = minor(dev);
	u_short tcr;

	if (mfpr(PR_MAPEN) == 0)
		return;

	/*
	 * If we are past boot stage, dz* will interrupt,
	 * therefore we block.
	 */
	s = spltty(); 
	tcr = dz->tcr;	/* remember which lines to scan */
	dz->tcr = (1 << mino);

	while ((dz->csr & DZ_CSR_TX_READY) == 0) /* Wait until ready */
		if (--timeout < 0)
			break;
	dz->tdr = ch;                    /* Put the character */
	timeout = 1<<15;
	while ((dz->csr & DZ_CSR_TX_READY) == 0) /* Wait until ready */
		if (--timeout < 0)
			break;

	dz->tcr = tcr;
	splx(s);
}

void 
dzcnpollc(dev, pollflag)
	dev_t dev;
	int pollflag;
{
	static	u_char mask;

	switch (vax_boardtype) {
#ifdef VAX60
	case VAX_BTYP_60:
		break;
#endif

	default:
		if (pollflag)
			mask = vsbus_setmask(0);
		else
			vsbus_setmask(mask);
		break;
	}
}
