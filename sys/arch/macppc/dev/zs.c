/*	$OpenBSD: zs.c,v 1.2 2001/09/27 02:13:36 mickey Exp $	*/
/*	$NetBSD: zs.c,v 1.17 2001/06/19 13:42:15 wiz Exp $	*/

/*
 * Copyright (c) 1996, 1998 Bill Studenmund
 * Copyright (c) 1995 Gordon W. Ross
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon Ross
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
 * Zilog Z8530 Dual UART driver (machine-dependent part)
 *
 * Runs two serial lines per chip using slave drivers.
 * Plain tty/async lines use the zs_async slave.
 * Sun keyboard/mouse uses the zs_kbd/zs_ms slaves.
 * Other ports use their own mice & keyboard slaves.
 *
 * Credits & history:
 *
 * With NetBSD 1.1, port-mac68k started using a port of the port-sparc
 * (port-sun3?) zs.c driver (which was in turn based on code in the
 * Berkeley 4.4 Lite release). Bill Studenmund did the port, with
 * help from Allen Briggs and Gordon Ross <gwr@netbsd.org>. Noud de
 * Brouwer field-tested the driver at a local ISP.
 *
 * Bill Studenmund and Gordon Ross then ported the machine-independant
 * z8530 driver to work with port-mac68k. NetBSD 1.2 contained an
 * intermediate version (mac68k using a local, patched version of
 * the m.i. drivers), with NetBSD 1.3 containing a full version.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <dev/cons.h>
#include <dev/ofw/openfirm.h>
#include <dev/ic/z8530reg.h>

#include <machine/z8530var.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>

/* Are these in a header file anywhere? */
/* Booter flags interface */
#define ZSMAC_RAW	0x01
#define ZSMAC_LOCALTALK	0x02

#define	PCLK	(9600 * 384)

#include "zsc.h"	/* get the # of zs chips defined */

/*
 * Some warts needed by z8530tty.c -
 */
int zs_def_cflag = (CREAD | CS8 | HUPCL);
int zs_major = 12;

/*
 * abort detection on console will now timeout after iterating on a loop
 * the following # of times. Cheep hack. Also, abort detection is turned
 * off after a timeout (i.e. maybe there's not a terminal hooked up).
 */
#define ZSABORT_DELAY 3000000

/* The layout of this is hardware-dependent (padding, order). */
struct zschan {
	volatile u_char	zc_csr;		/* ctrl,status, and indirect access */
	u_char		zc_xxx0[15];
	volatile u_char	zc_data;	/* data */
	u_char		zc_xxx1[15];
};
struct zsdevice {
	/* Yes, they are backwards. */
	struct	zschan zs_chan_b;
	struct	zschan zs_chan_a;
};

/* Flags from cninit() */
static int zs_hwflags[NZSC][2];
/* Default speed for each channel */
static int zs_defspeed[NZSC][2] = {
	{ 38400,	/* tty00 */
	  38400 },	/* tty01 */
};

/* console stuff */
void	*zs_conschan = 0;
#ifdef	ZS_CONSOLE_ABORT
int	zs_cons_canabort = 1;
#else
int	zs_cons_canabort = 0;
#endif /* ZS_CONSOLE_ABORT*/

/* device to which the console is attached--if serial. */
/* Mac stuff */

static int zs_get_speed __P((struct zs_chanstate *));

/*
 * Even though zsparam will set up the clock multiples, etc., we
 * still set them here as: 1) mice & keyboards don't use zsparam,
 * and 2) the console stuff uses these defaults before device
 * attach.
 */

static u_char zs_init_reg[16] = {
	0,	/* 0: CMD (reset, etc.) */
	0,	/* 1: No interrupts yet. */
	0,	/* IVECT */
	ZSWR3_RX_8 | ZSWR3_RX_ENABLE,
	ZSWR4_CLK_X16 | ZSWR4_ONESB | ZSWR4_EVENP,
	ZSWR5_TX_8 | ZSWR5_TX_ENABLE,
	0,	/* 6: TXSYNC/SYNCLO */
	0,	/* 7: RXSYNC/SYNCHI */
	0,	/* 8: alias for data port */
	ZSWR9_MASTER_IE,
	0,	/*10: Misc. TX/RX control bits */
	ZSWR11_TXCLK_BAUD | ZSWR11_RXCLK_BAUD,
	((PCLK/32)/38400)-2,	/*12: BAUDLO (default=38400) */
	0,			/*13: BAUDHI (default=38400) */
	ZSWR14_BAUD_ENA,
	ZSWR15_BREAK_IE,
};

/****************************************************************
 * Autoconfig
 ****************************************************************/

struct cfdriver zsc_cd = {
	NULL, "zsc", DV_TTY
};

/* Definition of the driver for autoconfig. */
int	zsc_match __P((struct device *, void *, void *));
void	zsc_attach __P((struct device *, struct device *, void *));
int	zsc_print __P((void *, const char *name));

struct cfattach zsc_ca = {
	sizeof(struct zsc_softc), zsc_match, zsc_attach
};

extern struct cfdriver zsc_cd;

int zshard __P((void *));
int zssoft __P((void *));
#ifdef ZS_TXDMA
static int zs_txdma_int __P((void *));
#endif

void zscnprobe __P((struct consdev *));
void zscninit __P((struct consdev *));
int  zscngetc __P((dev_t));
void zscnputc __P((dev_t, int));
void zscnpollc __P((dev_t, int));

/*
 * Is the zs chip present?
 */
int
zsc_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct confargs *ca = aux;
	struct cfdata *cf = match;

	if (strcmp(ca->ca_name, "escc") != 0)
		return 0;

	if (cf->cf_unit > 1)
		return 0;

	return 1;
}

/*
 * Attach a found zs.
 *
 * Match slave number to zs unit number, so that misconfiguration will
 * not set up the keyboard as ttya, etc.
 */
void
zsc_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct zsc_softc *zsc = (void *)self;
	struct confargs *ca = aux;
	struct zsc_attach_args zsc_args;
	volatile struct zschan *zc;
	struct zs_chanstate *cs;
	struct zsdevice *zsd;
	int channel;
	int s, theflags;
	int node, intr[3][3];
	u_int regs[16];

	zsd = mapiodev(ca->ca_baseaddr + ca->ca_reg[0], ca->ca_reg[1]);
	node = OF_child(ca->ca_node);	/* ch-a */

	for (channel = 0; channel < 2; channel++) {
		if (OF_getprop(node, "AAPL,interrupts",
			       intr[channel], sizeof(intr[0])) == -1 &&
		    OF_getprop(node, "interrupts",
			       intr[channel], sizeof(intr[0])) == -1) {
			printf(": cannot find interrupt property\n");
			return;
		}

		if (OF_getprop(node, "reg", regs, sizeof(regs)) < 24) {
			printf(": cannot find reg property\n");
			return;
		}
		regs[2] += ca->ca_baseaddr;
		regs[4] += ca->ca_baseaddr;
#ifdef ZS_TXDMA
		zsc->zsc_txdmareg[channel] = mapiodev(regs[2], regs[3]);
		zsc->zsc_txdmacmd[channel] =
			dbdma_alloc(sizeof(dbdma_command_t) * 3);
		memset(zsc->zsc_txdmacmd[channel], 0,
			sizeof(dbdma_command_t) * 3);
		dbdma_reset(zsc->zsc_txdmareg[channel]);
#endif
		node = OF_peer(node);	/* ch-b */
	}

	printf(": irq %d,%d\n", intr[0][0], intr[1][0]);

	/*
	 * Initialize software state for each channel.
	 */
	for (channel = 0; channel < 2; channel++) {
		zsc_args.channel = channel;
		zsc_args.hwflags = zs_hwflags[0][channel];
		cs = &zsc->zsc_cs[channel];

		cs->cs_channel = channel;
		cs->cs_private = NULL;
		cs->cs_ops = &zsops_null;

		zc = (channel == 0) ? &zsd->zs_chan_a : &zsd->zs_chan_b;

		cs->cs_reg_csr  = &zc->zc_csr;
		cs->cs_reg_data = &zc->zc_data;

		memcpy(cs->cs_creg, zs_init_reg, 16);
		memcpy(cs->cs_preg, zs_init_reg, 16);

		/* Current BAUD rate generator clock. */
		cs->cs_brg_clk = PCLK / 16;	/* RTxC is 230400*16, so use 230400 */
		if (zsc_args.hwflags & ZS_HWFLAG_CONSOLE)
			cs->cs_defspeed = zs_get_speed(cs);
		else
			cs->cs_defspeed = zs_defspeed[0][channel];
#ifdef NOTYET
		/* Define BAUD rate stuff. */
		xcs->cs_clocks[0].clk = PCLK;
		xcs->cs_clocks[0].flags = ZSC_RTXBRG | ZSC_RTXDIV;
		xcs->cs_clocks[1].flags =
			ZSC_RTXBRG | ZSC_RTXDIV | ZSC_VARIABLE | ZSC_EXTERN;
		xcs->cs_clocks[2].flags = ZSC_TRXDIV | ZSC_VARIABLE;
		xcs->cs_clock_count = 3;
		if (channel == 0) {
			theflags = 0; /*mac68k_machine.modem_flags;*/
			/*xcs->cs_clocks[1].clk = mac68k_machine.modem_dcd_clk;*/
			/*xcs->cs_clocks[2].clk = mac68k_machine.modem_cts_clk;*/
			xcs->cs_clocks[1].clk = 0;
			xcs->cs_clocks[2].clk = 0;
		} else {
			theflags = 0; /*mac68k_machine.print_flags;*/
			xcs->cs_clocks[1].flags = ZSC_VARIABLE;
			/*
			 * Yes, we aren't defining ANY clock source enables for the
			 * printer's DCD clock in. The hardware won't let us
			 * use it. But a clock will freak out the chip, so we
			 * let you set it, telling us to bar interrupts on the line.
			 */
			/*xcs->cs_clocks[1].clk = mac68k_machine.print_dcd_clk;*/
			/*xcs->cs_clocks[2].clk = mac68k_machine.print_cts_clk;*/
			xcs->cs_clocks[1].clk = 0;
			xcs->cs_clocks[2].clk = 0;
		}

		/* Set defaults in our "extended" chanstate. */
		xcs->cs_csource = 0;
		xcs->cs_psource = 0;
		xcs->cs_cclk_flag = 0;  /* Nothing fancy by default */
		xcs->cs_pclk_flag = 0;
#endif
		/*
		 * XXX - This might be better done with a "stub" driver
		 * (to replace zstty) that ignores LocalTalk for now.
		 */
		if (theflags & ZSMAC_LOCALTALK) {
			printf(" shielding from LocalTalk");
			cs->cs_defspeed = 1;
			cs->cs_creg[ZSRR_BAUDLO] = cs->cs_preg[ZSRR_BAUDLO] = 0xff;
			cs->cs_creg[ZSRR_BAUDHI] = cs->cs_preg[ZSRR_BAUDHI] = 0xff;
			zs_write_reg(cs, ZSRR_BAUDLO, 0xff);
			zs_write_reg(cs, ZSRR_BAUDHI, 0xff);
			/*
			 * If we might have LocalTalk, then make sure we have the
			 * Baud rate low-enough to not do any damage.
			 */
		}

		/*
		 * Look for a child driver for this channel.
		 * The child attach will setup the hardware.
		 */
		if (!config_found(self, (void *)&zsc_args, zsc_print)) {
			/* No sub-driver.  Just reset it. */
			u_char reset = (channel == 0) ?
				ZSWR9_A_RESET : ZSWR9_B_RESET;
			s = splzs();
			zs_write_reg(cs, 9, reset);
			splx(s);
		}
	}

	/* XXX - Now safe to install interrupt handlers. */
	mac_intr_establish(parent, intr[0][0], IST_LEVEL, IPL_TTY,
	    zshard, NULL, "zs");
	mac_intr_establish(parent, intr[1][0], IST_LEVEL, IPL_TTY,
	    zshard, NULL, "zs");
#ifdef ZS_TXDMA
	mac_intr_establish(parent, intr[0][1], IST_LEVEL, IPL_TTY,
	    zs_txdma_int, (void *)0, "zs");
	mac_intr_establish(parent, intr[1][1], IST_LEVEL, IPL_TTY,
	    zs_txdma_int, (void *)1, "zs");
#endif

	/*
	 * Set the master interrupt enable and interrupt vector.
	 * (common to both channels, do it on A)
	 */
	cs = &zsc->zsc_cs[0];
	s = splzs();
	/* interrupt vector */
	zs_write_reg(cs, 2, zs_init_reg[2]);
	/* master interrupt control (enable) */
	zs_write_reg(cs, 9, zs_init_reg[9]);
	splx(s);
}

int
zsc_print(aux, name)
	void *aux;
	const char *name;
{
	struct zsc_attach_args *args = aux;

	if (name != NULL)
		printf("%s: ", name);

	if (args->channel != -1)
		printf(" channel %d", args->channel);

	return UNCONF;
}

int
zsmdioctl(cs, cmd, data)
	struct zs_chanstate *cs;
	u_long cmd;
	caddr_t data;
{
	switch (cmd) {
	default:
		return (-1);
	}
	return (0);
}

void
zsmd_setclock(cs)
	struct zs_chanstate *cs;
{
#ifdef NOTYET
	struct xzs_chanstate *xcs = (void *)cs;

	if (cs->cs_channel != 0)
		return;

	/*
	 * If the new clock has the external bit set, then select the
	 * external source.
	 */
	via_set_modem((xcs->cs_pclk_flag & ZSC_EXTERN) ? 1 : 0);
#endif
}

static int zssoftpending;

/*
 * Our ZS chips all share a common, autovectored interrupt,
 * so we have to look at all of them on each interrupt.
 */
int
zshard(arg)
	void *arg;
{
	register struct zsc_softc *zsc;
	register int unit, rval;

	rval = 0;
	for (unit = 0; unit < zsc_cd.cd_ndevs; unit++) {
		zsc = zsc_cd.cd_devs[unit];
		if (zsc == NULL)
			continue;
		rval |= zsc_intr_hard(zsc);
		if (zsc->zsc_cs->cs_softreq)
		{
			/* zsc_req_softint(zsc); */
			/* We are at splzs here, so no need to lock. */
			if (zssoftpending == 0) {
				zssoftpending = 1;
				/* XXX setsoftserial(); */
			}
		}
	}
	return (rval);
}

/*
 * Similar scheme as for zshard (look at all of them)
 */
int
zssoft(arg)
	void *arg;
{
	register struct zsc_softc *zsc;
	register int unit;

	/* This is not the only ISR on this IPL. */
	if (zssoftpending == 0)
		return (0);

	/*
	 * The soft intr. bit will be set by zshard only if
	 * the variable zssoftpending is zero.
	 */
	zssoftpending = 0;

	for (unit = 0; unit < zsc_cd.cd_ndevs; ++unit) {
		zsc = zsc_cd.cd_devs[unit];
		if (zsc == NULL)
			continue;
		(void) zsc_intr_soft(zsc);
	}
	return (1);
}

#ifdef ZS_TXDMA
int
zs_txdma_int(arg)
	void *arg;
{
	int ch = (int)arg;
	struct zsc_softc *zsc;
	struct zs_chanstate *cs;
	int unit = 0;			/* XXX */
	extern int zstty_txdma_int();

	zsc = zsc_cd.cd_devs[unit];
	if (zsc == NULL)
		panic("zs_txdma_int");

	cs = zsc->zsc_cs[ch];
	zstty_txdma_int(cs);

	if (cs->cs_softreq) {
		if (zssoftpending == 0) {
			zssoftpending = 1;
			setsoftserial();
		}
	}
	return 1;
}

void
zs_dma_setup(cs, pa, len)
	struct zs_chanstate *cs;
	caddr_t pa;
	int len;
{
	struct zsc_softc *zsc;
	dbdma_command_t *cmdp;
	int ch = cs->cs_channel;

	zsc = zsc_cd.cd_devs[ch];
	cmdp = zsc->zsc_txdmacmd[ch];

	DBDMA_BUILD(cmdp, DBDMA_CMD_OUT_LAST, 0, len, kvtop(pa),
		DBDMA_INT_ALWAYS, DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);
	cmdp++;
	DBDMA_BUILD(cmdp, DBDMA_CMD_STOP, 0, 0, 0,
		DBDMA_INT_NEVER, DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);

	__asm __volatile("eieio");

	dbdma_start(zsc->zsc_txdmareg[ch], zsc->zsc_txdmacmd[ch]);
}
#endif

/*
 * Compute the current baud rate given a ZS channel.
 * XXX Assume internal BRG.
 */
int
zs_get_speed(cs)
	struct zs_chanstate *cs;
{
	int tconst;

	tconst = zs_read_reg(cs, 12);
	tconst |= zs_read_reg(cs, 13) << 8;
	return TCONST_TO_BPS(cs->cs_brg_clk, tconst);
}

/*
 * Read or write the chip with suitable delays.
 * MacII hardware has the delay built in.
 * No need for extra delay. :-) However, some clock-chirped
 * macs, or zsc's on serial add-on boards might need it.
 */
#define	ZS_DELAY()

u_char
zs_read_reg(cs, reg)
	struct zs_chanstate *cs;
	u_char reg;
{
	u_char val;

	out8(cs->cs_reg_csr, reg);
	ZS_DELAY();
	val = in8(cs->cs_reg_csr);
	ZS_DELAY();
	return val;
}

void
zs_write_reg(cs, reg, val)
	struct zs_chanstate *cs;
	u_char reg, val;
{
	out8(cs->cs_reg_csr, reg);
	ZS_DELAY();
	out8(cs->cs_reg_csr, val);
	ZS_DELAY();
}

u_char zs_read_csr(cs)
	struct zs_chanstate *cs;
{
	register u_char val;

	val = in8(cs->cs_reg_csr);
	ZS_DELAY();
	/* make up for the fact CTS is wired backwards */
	val ^= ZSRR0_CTS;
	return val;
}

void  zs_write_csr(cs, val)
	struct zs_chanstate *cs;
	u_char val;
{
	/* Note, the csr does not write CTS... */
	out8(cs->cs_reg_csr, val);
	ZS_DELAY();
}

u_char zs_read_data(cs)
	struct zs_chanstate *cs;
{
	register u_char val;

	val = in8(cs->cs_reg_data);
	ZS_DELAY();
	return val;
}

void  zs_write_data(cs, val)
	struct zs_chanstate *cs;
	u_char val;
{
	out8(cs->cs_reg_data, val);
	ZS_DELAY();
}

/****************************************************************
 * Console support functions (powermac specific!)
 * Note: this code is allowed to know about the layout of
 * the chip registers, and uses that to keep things simple.
 * XXX - I think I like the mvme167 code better. -gwr
 * XXX - Well :-P  :-)  -wrs
 ****************************************************************/

#define zscnpollc	nullcnpollc
cons_decl(zs);

static void	zs_putc __P((register volatile struct zschan *, int));
static int	zs_getc __P((register volatile struct zschan *));
extern int	zsopen __P(( dev_t dev, int flags, int mode, struct proc *p));

static int stdin, stdout;

/*
 * Console functions.
 */

/*
 * zscnprobe is the routine which gets called as the kernel is trying to
 * figure out where the console should be. Each io driver which might
 * be the console (as defined in mac68k/conf.c) gets probed. The probe
 * fills in the consdev structure. Important parts are the device #,
 * and the console priority. Values are CN_DEAD (don't touch me),
 * CN_NORMAL (I'm here, but elsewhere might be better), CN_INTERNAL
 * (the video, better than CN_NORMAL), and CN_REMOTE (pick me!)
 *
 * As the mac's a bit different, we do extra work here. We mainly check
 * to see if we have serial echo going on. Also chould check for default
 * speeds.
 */

/*
 * Polled input char.
 */
int
zs_getc(zc)
	register volatile struct zschan *zc;
{
	register int s, c, rr0;

	s = splhigh();
	/* Wait for a character to arrive. */
	do {
		rr0 = in8(&zc->zc_csr);
		ZS_DELAY();
	} while ((rr0 & ZSRR0_RX_READY) == 0);

	c = in8(&zc->zc_data);
	ZS_DELAY();
	splx(s);

	/*
	 * This is used by the kd driver to read scan codes,
	 * so don't translate '\r' ==> '\n' here...
	 */
	return (c);
}

/*
 * Polled output char.
 */
void
zs_putc(zc, c)
	register volatile struct zschan *zc;
	int c;
{
	register int s, rr0;
	register long wait = 0;

	s = splhigh();
	/* Wait for transmitter to become ready. */
	do {
		rr0 = in8(&zc->zc_csr);
		ZS_DELAY();
	} while (((rr0 & ZSRR0_TX_READY) == 0) && (wait++ < 1000000));

	if ((rr0 & ZSRR0_TX_READY) != 0) {
		out8(&zc->zc_data, c);
		ZS_DELAY();
	}
	splx(s);
}


/*
 * Polled console input putchar.
 */
int
zscngetc(dev)
	dev_t dev;
{
	register volatile struct zschan *zc = zs_conschan;
	register int c;

	if (zc) {
		c = zs_getc(zc);
	} else {
		char ch = 0;
		OF_read(stdin, &ch, 1);
		c = ch;
	}
	return c;
}

/*
 * Polled console output putchar.
 */
void
zscnputc(dev, c)
	dev_t dev;
	int c;
{
	register volatile struct zschan *zc = zs_conschan;

	if (zc) {
		zs_putc(zc, c);
	} else {
		char ch = c;
		OF_write(stdout, &ch, 1);
	}
}

extern int ofccngetc __P((dev_t));
extern void ofccnputc __P((dev_t, int));

struct consdev consdev_zs = {
	zscnprobe,
	zscninit,
	zscngetc,
	zscnputc,
	zscnpollc,
	NULL,
};

void
zscnprobe(cp)
	struct consdev *cp;
{
	int chosen, pkg;
	int unit = 0;
	char name[16];

	if ((chosen = OF_finddevice("/chosen")) == -1)
		return;

	if (OF_getprop(chosen, "stdin", &stdin, sizeof(stdin)) == -1)
		return;
	if (OF_getprop(chosen, "stdout", &stdout, sizeof(stdout)) == -1)
		return;

	if ((pkg = OF_instance_to_package(stdin)) == -1)
		return;

	memset(name, 0, sizeof(name));
	if (OF_getprop(pkg, "device_type", name, sizeof(name)) == -1)
		return;

	if (strcmp(name, "serial") != 0)
		return;

	memset(name, 0, sizeof(name));
	if (OF_getprop(pkg, "name", name, sizeof(name)) == -1)
		return;

	if (strcmp(name, "ch-b") == 0)
		unit = 1;

	cp->cn_dev = makedev(zs_major, unit);
	cp->cn_pri = CN_REMOTE;
}

void
zscninit(cp)
	struct consdev *cp;
{
	int escc, escc_ch, obio, zs_offset;
	int ch = 0;
	u_int32_t reg[5];
	char name[16];

	if ((escc_ch = OF_instance_to_package(stdin)) == -1)
		return;

	memset(name, 0, sizeof(name));
	if (OF_getprop(escc_ch, "name", name, sizeof(name)) == -1)
		return;

	if (strcmp(name, "ch-b") == 0)
		ch = 1;

	if (OF_getprop(escc_ch, "reg", reg, sizeof(reg)) < 4)
		return;
	zs_offset = reg[0];

	escc = OF_parent(escc_ch);
	obio = OF_parent(escc);

	if (OF_getprop(obio, "assigned-addresses", reg, sizeof(reg)) < 12)
		return;
	zs_conschan = (void *)(reg[2] + zs_offset);

	zs_hwflags[0][ch] = ZS_HWFLAG_CONSOLE;
}

void
zs_abort()
{

}
