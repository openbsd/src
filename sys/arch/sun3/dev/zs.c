/*	$NetBSD: zs.c,v 1.31 1996/01/24 22:40:25 gwr Exp $	*/

/*
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
#include <dev/ic/z8530reg.h>
#include <machine/z8530var.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/eeprom.h>
#include <machine/isr.h>
#include <machine/obio.h>
#include <machine/mon.h>

/*
 * XXX: Hard code this to make console init easier...
 */
#define	NZS	2		/* XXX */


/* The Sun3 provides a 4.9152 MHz clock to the ZS chips. */
#define PCLK	(9600 * 512)	/* PCLK pin input clock rate */

/*
 * Define interrupt levels.
 */
#define ZSHARD_PRI	6	/* Wired on the CPU board... */
#define ZSSOFT_PRI	3	/* Want tty pri (4) but this is OK. */


/* The layout of this is hardware-dependent (padding, order). */
struct zschan {
	volatile u_char	zc_csr;		/* ctrl,status, and indirect access */
	u_char		zc_xxx0;
	volatile u_char	zc_data;	/* data */
	u_char		zc_xxx1;
};
struct zsdevice {
	/* Yes, they are backwards. */
	struct	zschan zs_chan_b;
	struct	zschan zs_chan_a;
};


/* Default OBIO addresses. */
static int zs_physaddr[NZS] = { OBIO_KEYBD_MS, OBIO_ZS };
/* Saved PROM mappings */
static struct zsdevice *zsaddr[NZS];	/* See zs_init() */
/* Flags from cninit() */
static int zs_hwflags[NZS][2];
/* Default speed for each channel */
static int zs_defspeed[NZS][2] = {
	{ 1200, 	/* keyboard */
	  1200 },	/* mouse */
	{ 9600, 	/* ttya */
	  9600 },	/* ttyb */
};


/* Find PROM mappings (for console support). */
void zs_init()
{
	int i;

	for (i = 0; i < NZS; i++) {
		zsaddr[i] = (struct zsdevice *)
			obio_find_mapping(zs_physaddr[i], OBIO_ZS_SIZE);
	}
}	


struct zschan *
zs_get_chan_addr(zsc_unit, channel)
	int zsc_unit, channel;
{
	struct zsdevice *addr;
	struct zschan *zc;

	if (zsc_unit >= NZS)
		return NULL;
	addr = zsaddr[zsc_unit];
	if (addr == NULL)
		return NULL;
	if (channel == 0) {
		zc = &addr->zs_chan_a;
	} else {
		zc = &addr->zs_chan_b;
	}
	return (zc);
}


static u_char zs_init_reg[16] = {
	0,	/* 0: CMD (reset, etc.) */
	ZSWR1_RIE | ZSWR1_TIE | ZSWR1_SIE,
	0x18 + ZSHARD_PRI,	/* IVECT */
	ZSWR3_RX_8 | ZSWR3_RX_ENABLE,
	ZSWR4_CLK_X16 | ZSWR4_ONESB | ZSWR4_EVENP,
	ZSWR5_TX_8 | ZSWR5_TX_ENABLE,
	0,	/* 6: TXSYNC/SYNCLO */
	0,	/* 7: RXSYNC/SYNCHI */
	0,	/* 8: alias for data port */
	ZSWR9_MASTER_IE,
	0,	/*10: Misc. TX/RX control bits */
	ZSWR11_TXCLK_BAUD | ZSWR11_RXCLK_BAUD,
	14,	/*12: BAUDLO (default=9600) */
	0,	/*13: BAUDHI (default=9600) */
	ZSWR14_BAUD_FROM_PCLK | ZSWR14_BAUD_ENA,
	ZSWR15_BREAK_IE | ZSWR15_DCD_IE,
};


/****************************************************************
 * Autoconfig
 ****************************************************************/

/* Definition of the driver for autoconfig. */
static int	zsc_match(struct device *, void *, void *);
static void	zsc_attach(struct device *, struct device *, void *);

struct cfdriver zsccd = {
	NULL, "zsc", zsc_match, zsc_attach,
	DV_DULL, sizeof(struct zsc_softc), NULL,
};

static int zshard(void *);
static int zssoft(void *);


/*
 * Is the zs chip present?
 */
static int
zsc_match(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	int unit, x;
	void *zsva;

	unit = cf->cf_unit;
	if (unit < 0 || unit >= NZS)
		return (0);

	/* Make sure zs_init() found mappings. */
	zsva = zsaddr[unit];
	if (zsva == NULL)
		return (0);

	if (ca->ca_paddr == -1)
		ca->ca_paddr = zs_physaddr[unit];
	if (ca->ca_intpri == -1)
		ca->ca_intpri = ZSHARD_PRI;

	/* This returns -1 on a fault (bus error). */
	x = peek_byte(zsva);
	return (x != -1);
}

static int
zsc_print(aux, name)
	void *aux;
	char *name;
{
	struct zsc_attach_args *args = aux;

	if (name != NULL)
		printf("%s: ", name);

	if (args->channel != -1)
		printf(" channel %d", args->channel);

	return UNCONF;
}

/*
 * Attach a found zs.
 *
 * Match slave number to zs unit number, so that misconfiguration will
 * not set up the keyboard as ttya, etc.
 */
static void
zsc_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct zsc_softc *zsc = (void *) self;
	struct confargs *ca = aux;
	struct zsc_attach_args zsc_args;
	volatile struct zschan *zc;
	struct zs_chanstate *cs;
	int zsc_unit, channel;
	int reset, s;
	static int didintr;

	zsc_unit = zsc->zsc_dev.dv_unit;

	printf(" softpri %d\n", ZSSOFT_PRI);

	/* Use the mapping setup by the Sun PROM. */
	if (zsaddr[zsc_unit] == NULL)
		panic("zs_attach: zs%d not mapped\n", zsc_unit);

	/*
	 * Initialize software state for each channel.
	 */
	for (channel = 0; channel < 2; channel++) {
		cs = &zsc->zsc_cs[channel];

		zc = zs_get_chan_addr(zsc_unit, channel);
		cs->cs_reg_csr  = &zc->zc_csr;
		cs->cs_reg_data = &zc->zc_data;

		cs->cs_channel = channel;
		cs->cs_private = NULL;
		cs->cs_ops = &zsops_null;

		/* Define BAUD rate clock for the MI code. */
		cs->cs_pclk_div16 = PCLK / 16;

		/* XXX: get defspeed from EEPROM instead? */
		cs->cs_defspeed = zs_defspeed[zsc_unit][channel];

		bcopy(zs_init_reg, cs->cs_creg, 16);
		bcopy(zs_init_reg, cs->cs_preg, 16);

		/*
		 * Clear the master interrupt enable.
		 * The INTENA is common to both channels,
		 * so just do it on the A channel.
		 */
		if (channel == 0) {
			ZS_WRITE(cs, 9, 0);
		}

		/*
		 * Look for a child driver for this channel.
		 * The child attach will setup the hardware.
		 */
		zsc_args.channel = channel;
		zsc_args.hwflags = zs_hwflags[zsc_unit][channel];
		if (!config_found(self, (void *) &zsc_args, zsc_print)) {
			/* No sub-driver.  Just reset it. */
			reset = (channel == 0) ?
				ZSWR9_A_RESET : ZSWR9_B_RESET;
			s = splzs();
			ZS_WRITE(cs,  9, reset);
			splx(s);
		}
	}

	/* Now safe to install interrupt handlers */
	if (!didintr) {
		didintr = 1;
		isr_add_autovect(zssoft, NULL, ZSSOFT_PRI);
		isr_add_autovect(zshard, NULL, ZSHARD_PRI);
	}

	/*
	 * Set the master interrupt enable and interrupt vector.
	 * (common to both channels, do it on A)
	 */
	cs = &zsc->zsc_cs[0];
	s = splzs();
	/* interrupt vector */
	ZS_WRITE(cs, 2, zs_init_reg[2]);
	/* master interrupt control (enable) */
	ZS_WRITE(cs, 9, zs_init_reg[9]);
	splx(s);
}

static int
zshard(arg)
	void *arg;
{
	struct zsc_softc *zsc;
	int unit, rval;
	
	/* Do ttya/ttyb first, because they go faster. */
	rval = 0;
	unit = zsccd.cd_ndevs;
	while (--unit >= 0) {
		zsc = zsccd.cd_devs[unit];
		if (zsc != NULL) {
			rval |= zsc_intr_hard(zsc);
		}
	}
	return (rval);
}

int zssoftpending;

void
zsc_req_softint(zsc)
	struct zsc_softc *zsc;
{	
	if (zssoftpending == 0) {
		/* We are at splzs here, so no need to lock. */
		zssoftpending = ZSSOFT_PRI;
		isr_soft_request(ZSSOFT_PRI);
	}
}

static int
zssoft(arg)
	void *arg;
{
	struct zsc_softc *zsc;
	int unit;

	/* This is not the only ISR on this IPL. */
	if (zssoftpending == 0)
		return (0);

	/*
	 * The soft intr. bit will be set by zshard only if
	 * the variable zssoftpending is zero.  The order of
	 * these next two statements prevents our clearing
	 * the soft intr bit just after zshard has set it.
	 */
	isr_soft_clear(ZSSOFT_PRI);
	zssoftpending = 0;

	/* Do ttya/ttyb first, because they go faster. */
	unit = zsccd.cd_ndevs;
	while (--unit >= 0) {
		zsc = zsccd.cd_devs[unit];
		if (zsc != NULL) {
			(void) zsc_intr_soft(zsc);
		}
	}
	return (1);
}


/*
 * Read or write the chip with suitable delays.
 */

u_char
zs_read_reg(cs, reg)
	struct zs_chanstate *cs;
	u_char reg;
{
	u_char val;

	*cs->cs_reg_csr = reg;
	ZS_DELAY();
	val = *cs->cs_reg_csr;
	ZS_DELAY();
	return val;
}

void
zs_write_reg(cs, reg, val)
	struct zs_chanstate *cs;
	u_char reg, val;
{
	*cs->cs_reg_csr = reg;
	ZS_DELAY();
	*cs->cs_reg_csr = val;
	ZS_DELAY();
}

/****************************************************************
 * Console support functions (Sun3 specific!)
 ****************************************************************/

/*
 * Polled input char.
 */
int
zs_getc(arg)
	void *arg;
{
	register volatile struct zschan *zc = arg;
	register int s, c, rr0;

	s = splhigh();
	/* Wait for a character to arrive. */
	do {
		rr0 = zc->zc_csr;
		ZS_DELAY();
	} while ((rr0 & ZSRR0_RX_READY) == 0);

	c = zc->zc_data;
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
zs_putc(arg, c)
	void *arg;
	int c;
{
	register volatile struct zschan *zc = arg;
	register int s, rr0;

	s = splhigh();
	/* Wait for transmitter to become ready. */
	do {
		rr0 = zc->zc_csr;
		ZS_DELAY();
	} while ((rr0 & ZSRR0_TX_READY) == 0);

	zc->zc_data = c;
	ZS_DELAY();
	splx(s);
}

extern struct consdev consdev_kd;	/* keyboard/display */
extern struct consdev consdev_tty;
extern struct consdev *cn_tab;	/* physical console device info */
extern void nullcnpollc();

void *zs_conschan;

/*
 * This function replaces sys/dev/cninit.c
 * Determine which device is the console using
 * the "console" byte from the EEPROM.
 */
void
cninit()
{
	struct zschan *zc;
	struct consdev *cn;
	int zsc_unit, channel;

	switch (ee_console) {

	case EE_CONS_TTYA:
	case EE_CONS_TTYB:
		zsc_unit = 1;
		channel = (ee_console & 1);
		cn = &consdev_tty;
		cn->cn_dev = makedev(ZSTTY_MAJOR, channel);
		cn->cn_pri = CN_REMOTE;
		break;

	default:
		mon_printf("cninit: unknown eeprom console setting\n");
		/* assume keyboard/display */
		/* fallthrough */
	case EE_CONS_BW:
	case EE_CONS_COLOR:
	case EE_CONS_P4OPT:
		zsc_unit = 0;
		channel = 0;
		cn = &consdev_kd;
		/* Set cn_dev, cn_pri in kd.c */
		break;
	}

	zc = zs_get_chan_addr(zsc_unit, channel);
	if (zc == NULL) {
		mon_printf("cninit: zs not mapped.\n");
		return;
	}
	zs_conschan = zc;
	zs_hwflags[zsc_unit][channel] = ZS_HWFLAG_CONSOLE;
	cn_tab = cn;
	(*cn->cn_init)(cn);
}


/* We never call this. */
void
nullcnprobe(cn)
	struct consdev *cn;
{
}

void
zscninit(cn)
	struct consdev *cn;
{
	int unit = minor(cn->cn_dev) & 1;

	mon_printf("console is zstty%d (tty%c)\n",
		   unit, unit + 'a');
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

	c = zs_getc(zc);
	return (c);
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

	zs_putc(zc, c);
}


struct consdev consdev_tty = {
	nullcnprobe,
	zscninit,
	zscngetc,
	zscnputc,
	nullcnpollc,
};


/*
 * Handle user request to enter kernel debugger.
 */
void
zs_abort()
{
	register volatile struct zschan *zc = zs_conschan;
	int rr0;

	/* Wait for end of break to avoid PROM abort. */
	/* XXX - Limit the wait? */
	do {
		rr0 = zc->zc_csr;
		ZS_DELAY();
	} while (rr0 & ZSRR0_BREAK);

	/* XXX - Always available, but may be the PROM monitor. */
	Debugger();
}
