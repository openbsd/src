/*	$OpenBSD: zs.c,v 1.11 1998/05/03 07:13:03 gene Exp $	*/
/*	$NetBSD: zs.c,v 1.12 1996/12/18 05:04:22 scottr Exp $	*/

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
#include "z8530reg.h"
#include <machine/z8530var.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/viareg.h>

/*
 * XXX: Hard code this to make console init easier...
 */
#define	NZSC	1		/* XXX */

/*
 * Define interrupt levels.
 */
#define ZSHARD_PRI	6	/* Wired on the CPU board... */
#define ZSSOFT_PRI	3	/* Want tty pri (4) but this is OK. */

/* The layout of this is hardware-dependent (padding, order). */
struct zschan {
	volatile u_char	zc_csr;		/* ctrl,status, and indirect access */
	u_char		zc_xxx0;
	u_char		zc_xxx1;
	u_char		zc_xxx2;
	volatile u_char	zc_data;	/* data */
	u_char		zc_xxx3;
	u_char		zc_xxx4;
	u_char		zc_xxx5;
};
/*
 * The zsdevice structure is not used on the mac68k port as the
 * chip is wired up weird. Channel B & A are interspursed with
 * the data & control bytes
struct zsdevice {
	/! Yes, they are backwards. !/
	struct	zschan zs_chan_b;
	struct	zschan zs_chan_a;
};
*/

/* Saved PROM mappings */
static char *zsaddr[NZSC];	/* See zs_init() */
/* Flags from cninit() */
static int zs_hwflags[NZSC][2];
/* Default speed for each channel */
static int zs_defspeed[NZSC][2] = {
	{ 9600, 	/* tty00 */
	  9600 },	/* tty01 */
};
/* console stuff */
void *zs_conschan = 0;
int   zs_consunit;
/* device that the console is attached to--if serial. */
dev_t	mac68k_zsdev;
/* Mac stuff, some vestages of old mac serial driver here */
volatile unsigned char *sccA = 0;

static struct zschan	*zs_get_chan_addr __P((int zsc_unit, int channel));
void			zs_init __P((void));

static struct zschan *
zs_get_chan_addr(zsc_unit, channel)
	int zsc_unit, channel;
{
	char *addr;
	struct zschan *zc;

	if (zsc_unit >= NZSC)
		return NULL;
	addr = zsaddr[zsc_unit];
	if (addr == NULL)
		return NULL;
	if (channel == 0) {
		zc = (struct zschan *)(addr +2);
		/* handle the fact the ports are intertwined. */
	} else {
		zc = (struct zschan *)(addr);
	}
	return (zc);
}


/* Find PROM mappings (for console support). */
static int zsinited = 0; /* 0 = not, 1 = inited, not attached, 2= attached */

void
zs_init()
{
	if ((zsinited == 2)&&(zsaddr[0] != (char *) sccA))
		panic("Moved zs0 address after attached!");
	zsaddr[0] = (char *) sccA;
	zsinited = 1;
	if (zs_conschan != 0){ /* we might have moved io under the console */
		zs_conschan = zs_get_chan_addr(0, zs_consunit);
		/* so recalc the console port */
	}
}	


/*
 * Even though zsparam will set up the clock multiples, etc., we
 * still set them here as: 1) mice & keyboards don't use zsparam,
 * and 2) the console stuff uses these defaults before device
 * attach.
 */

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
	ZSWR14_BAUD_ENA,
	ZSWR15_BREAK_IE | ZSWR15_DCD_IE | ZSWR15_CTS_IE,
};


/****************************************************************
 * Autoconfig
 ****************************************************************/

/* Definition of the driver for autoconfig. */
static int	zsc_match __P((struct device *, void *, void *));
static void	zsc_attach __P((struct device *, struct device *, void *));
static int	zsc_print __P((void *aux, const char *name));

struct cfattach zsc_ca = {
	sizeof(struct zsc_softc), zsc_match, zsc_attach
};

struct cfdriver zsc_cd = {
	NULL, "zsc", DV_DULL
};

int	zshard __P((void *));
int	zssoft __P((void *));


/*
 * Is the zs chip present?
 */
static int
zsc_match(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	return 1;
}

static int
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
	struct zsc_attach_args zsc_args;
	volatile struct zschan *zc;
	struct zs_chanstate *cs;
	int zsc_unit, channel;
	int reset, s;
	int chip = 0;	/* XXX quiet bogus gcc warning */

	if (!zsinited) zs_init();
	zsinited = 2;

	zsc_unit = zsc->zsc_dev.dv_unit;

	/* Make sure everything's inited ok. */
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
		cs->cs_pclk_div16 = mac68k_machine.sccClkConst*2;
		cs->cs_csource = 0;
		cs->cs_psource = 0;

		cs->cs_defspeed = zs_defspeed[zsc_unit][channel];

		bcopy(zs_init_reg, cs->cs_creg, 16);
		bcopy(zs_init_reg, cs->cs_preg, 16);

		/*
		 * Clear the master interrupt enable.
		 * The INTENA is common to both channels,
		 * so just do it on the A channel.
		 */
		if (channel == 0) {
			zs_write_reg(cs, 9, 0);

			chip = 0; /* We'll turn chip checking on post 1.2 */
			printf(" chip type %d \n",chip);
		}
		cs->cs_chip = chip;

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
			zs_write_reg(cs,  9, reset);
			splx(s);
		}
	}

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

void
zstty_mdattach(zsc, zst, cs, tp)
	struct zsc_softc *zsc;
	struct zstty_softc *zst;
	struct zs_chanstate *cs;
	struct tty *tp;
{
	int theflags;

	zst->zst_resetdef = 0;
	cs->cs_clock_count = 3; /* internal + externals */
	cs->cs_cclk_flag = 0;  /* Not doing anything fancy by default */
	cs->cs_pclk_flag = 0;
	cs->cs_clocks[0].clk = mac68k_machine.sccClkConst*32;
	cs->cs_clocks[0].flags = ZSC_RTXBRG; /* allowing divide by 16 will
					melt the driver! */

	cs->cs_clocks[1].flags = ZSC_RTXBRG | ZSC_RTXDIV | ZSC_VARIABLE | ZSC_EXTERN;
	cs->cs_clocks[2].flags = ZSC_TRXDIV | ZSC_VARIABLE;
	if (zst->zst_dev.dv_unit == 0) {
		theflags = mac68k_machine.modem_flags;
		cs->cs_clocks[1].clk = mac68k_machine.modem_dcd_clk;
		cs->cs_clocks[2].clk = mac68k_machine.modem_cts_clk;
	} else if (zst->zst_dev.dv_unit == 1) {
		theflags = mac68k_machine.print_flags;
		cs->cs_clocks[1].flags = ZSC_VARIABLE;
		/*
		 * Yes, we aren't defining ANY clock source enables for the
		 * printer's DCD clock in. The hardware won't let us
		 * use it. But a clock will freak out the chip, so we
		 * let you set it, telling us to bar interrupts on the line.
		 */
		cs->cs_clocks[1].clk = mac68k_machine.print_dcd_clk;
		cs->cs_clocks[2].clk = mac68k_machine.print_cts_clk;
	} else
		theflags = 0;

	if (cs->cs_clocks[1].clk)
		zst->zst_hwflags |= ZS_HWFLAG_IGDCD;
	if (cs->cs_clocks[2].clk)
		zst->zst_hwflags |= ZS_HWFLAG_IGCTS;

	if (theflags & ZSMAC_RAW) {
		zst->zst_cflag = ZSTTY_RAW_CFLAG;
		zst->zst_iflag = ZSTTY_RAW_IFLAG;
		zst->zst_lflag = ZSTTY_RAW_LFLAG;
		zst->zst_oflag = ZSTTY_RAW_OFLAG;
		printf(" (raw defaults)");
	}
	if (theflags & ZSMAC_LOCALTALK) {
		printf(" shielding from LocalTalk");
		zst->zst_ospeed = tp->t_ospeed = 1;
		zst->zst_ispeed = tp->t_ispeed = 1;
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

	/* For the mac, we have rtscts = check CTS for output control, no
	 * input control. mdmbuf means check DCD for output, and use DTR
	 * for input control. mdmbuf & rtscts means use CTS for output
	 * control, and DTR for input control. */

	zst->zst_hwimasks[1] = 0;
	zst->zst_hwimasks[2] = ZSWR5_DTR;
	zst->zst_hwimasks[3] = ZSWR5_DTR;
}

int
zsmdioctl(tp, com, data, flag, p)
	struct tty *tp;
	u_long com;
	caddr_t data;
	int flag;
	struct proc *p;
{
	return (-1);
}

void
zsmd_setclock(cs)
	struct zs_chanstate *cs;
{
	if (cs->cs_channel != 0)
		return;
	/*
	 * If the new clock has the external bit set, then select the
	 * external source.
	 */
	via_set_modem((cs->cs_pclk_flag & ZSC_EXTERN) ? 1 : 0);
}

int
zshard(arg)
	void *arg;
{
	struct zsc_softc *zsc;
	int unit, rval;
#ifdef ZSMACDEBUG
	itecnputc(mac68k_zsdev, 'Z');
#endif
	
	rval = 0;
	unit = zsc_cd.cd_ndevs;
	while (--unit >= 0) {
		zsc = zsc_cd.cd_devs[unit];
		if (zsc != NULL) {
			rval |= zsc_intr_hard(zsc);
		}
	}
#ifdef ZSMACDEBUG
	itecnputc(mac68k_zsdev, '\n');
#endif
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
	/*	isr_soft_request(ZSSOFT_PRI); */
		setsoftserial();
	}
}

int
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
/*	isr_soft_clear(ZSSOFT_PRI); */
	zssoftpending = 0;

	/* Do ttya/ttyb first, because they go faster. */
	unit = zsc_cd.cd_ndevs;
	while (--unit >= 0) {
		zsc = zsc_cd.cd_devs[unit];
		if (zsc != NULL) {
			(void) zsc_intr_soft(zsc);
		}
	}
	return (1);
}


/*
 * Read or write the chip with suitable delays.
 */
#define	ZS_DELAY()
/*
 * MacII hardware has the delay built in. No need for extra delay. :-)
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

u_char zs_read_csr(cs)
	struct zs_chanstate *cs;
{
	register u_char v;

	v = (*cs->cs_reg_csr) ^ ZSRR0_CTS;
	/* make up for the fact CTS is wired backwards */
	ZS_DELAY();
	return v;
}

u_char zs_read_data(cs)
	struct zs_chanstate *cs;
{
	register u_char v;

	v = *cs->cs_reg_data;
	ZS_DELAY();
	return v;
}

void  zs_write_csr(cs, val)
	struct zs_chanstate *cs;
	u_char val;
{
	*cs->cs_reg_csr = val;
	ZS_DELAY();
}

void  zs_write_data(cs, val)
	struct zs_chanstate *cs;
	u_char val;
{
	*cs->cs_reg_data = val;
	ZS_DELAY();
}

/****************************************************************
 * Console support functions (Originally Sun3 specific!)
 * Now works w/ just mac68k port!
 ****************************************************************/

#define zscnpollc	nullcnpollc
cons_decl(zs);

static void	zs_putc __P((register volatile struct zschan *, int));
static int	zs_getc __P((register volatile struct zschan *));
static void	zscnsetup __P((void));
extern int	zsopen __P(( dev_t dev, int flags, int mode, struct proc *p));

/*
 * Console functions.
 */

/*
 * This code modled after the zs_setparam routine in zskgdb
 * It sets the console unit to a known state so we can output
 * correctly.
 */
static void
zscnsetup()
{
	struct zs_chanstate cs;
	struct zschan *zc;
	int    tconst, s;
	
	/* Setup temporary chanstate. */
	bzero((caddr_t)&cs, sizeof(cs));
	zc = zs_conschan;
	cs.cs_reg_csr  = &zc->zc_csr;
	cs.cs_reg_data = &zc->zc_data;
	cs.cs_channel = zs_consunit;

	bcopy(zs_init_reg, cs.cs_preg, 16);
	tconst = BPS_TO_TCONST(mac68k_machine.sccClkConst*2, zs_defspeed[0][zs_consunit]);
        cs.cs_preg[5] |= ZSWR5_DTR | ZSWR5_RTS;
	cs.cs_preg[1] = 0; /* don't enable interrupts */
        cs.cs_preg[12] = tconst;
        cs.cs_preg[13] = tconst >> 8;

        s = splhigh();
        zs_loadchannelregs(&cs);
        splx(s);
}

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
 * to see if we have serial echo going on, and if the tty's are supposed
 * to default to raw or not.
 */
void
zscnprobe(struct consdev * cp)
{
        extern u_long   IOBase;
        int     maj, unit;

        for (maj = 0; maj < nchrdev; maj++) {
                if (cdevsw[maj].d_open == zsopen) {
                        break;
                }
        }
        if (maj == nchrdev) {
                /* no console entry for us */
                if (mac68k_machine.serial_boot_echo) {
                        mac68k_set_io_offsets(IOBase);
                	zs_conschan = (struct zschan *) -1; /* dummy flag for zs_init() */
			zs_consunit = 1;
			zs_hwflags[0][zs_consunit] = ZS_HWFLAG_CONSOLE;
			zs_init();
                        zscnsetup();
                }
                return;
        }

        cp->cn_pri = CN_NORMAL;                 /* Lower than CN_INTERNAL */
        if (mac68k_machine.serial_console != 0) {
                cp->cn_pri = CN_REMOTE;         /* Higher than CN_INTERNAL */
                mac68k_machine.serial_boot_echo =0;
        }

        unit = (mac68k_machine.serial_console == 1) ? 0 : 1;
	zs_consunit = unit;

        mac68k_zsdev = cp->cn_dev = makedev(maj, unit);

        if (mac68k_machine.serial_boot_echo) {
                /*
                 * at this point, we know that we don't have a serial
                 * console, but are doing echo
                 */
                mac68k_set_io_offsets(IOBase);
                zs_conschan = (struct zschan *) -1; /* dummy flag for zs_init() */
		zs_consunit = 1;
		zs_hwflags[0][zs_consunit] = ZS_HWFLAG_CONSOLE;
		zs_init();
                zscnsetup();
        }
        return;
}

void
zscninit(struct consdev * cp)
{
        extern u_long   IOBase;
	int chan = minor(cp->cn_dev & 1);

        mac68k_set_io_offsets(IOBase);
	zs_conschan = (struct zschan *) -1;
	zs_consunit = chan;
	zs_hwflags[0][zs_consunit] = ZS_HWFLAG_CONSOLE;
#ifdef ZS_CONSOLE_ABORT
	zs_hwflags[0][zs_consunit] |= ZS_HWFLAG_CONABRT;
#endif
	zs_init();
        /*
	 * zsinit will set up the addresses of the scc. It will also, if
	 * zs_conschan != 0, calculate the new address of the conschan for
	 * unit zs_consunit. So zs_init implicitly sets zs_conschan to the right
	 * number. :-)
         */
        zscnsetup();
        printf("\nOpenBSD/mac68k console\n");
}


/*
 * Polled input char.
 */
static int
zs_getc(zc)
	register volatile struct zschan *zc;
{
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
static void
zs_putc(zc, c)
	register volatile struct zschan *zc;
	int c;
{
	register int s, rr0;
	register long wait = 0;

	s = splhigh();
	/* Wait for transmitter to become ready. */
	do {
		rr0 = zc->zc_csr;
		ZS_DELAY();
	} while (((rr0 & ZSRR0_TX_READY) == 0) && (wait++ < 1000000));

	if ((rr0 & ZSRR0_TX_READY) != 0) {
		zc->zc_data = c;
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



/*
 * Handle user request to enter kernel debugger.
 */
void
zs_abort(zst)
	register struct zstty_softc *zst;
{
	register volatile struct zschan *zc = zs_conschan;
	int rr0;
	register long wait = 0;

	if ((zst->zst_hwflags & ZS_HWFLAG_CONABRT) == 0)
		return;

	/* Wait for end of break to avoid PROM abort. */
	/* XXX - Limit the wait? */
	do {
		rr0 = zc->zc_csr;
		ZS_DELAY();
	} while ((rr0 & ZSRR0_BREAK) && (wait++ < ZSABORT_DELAY));

	if (wait > ZSABORT_DELAY) {
		if (zst != NULL) zst->zst_hwflags &= ~ZS_HWFLAG_CONABRT;
	/* If we time out, turn off the abort ability! */
	}
#ifdef DDB
	Debugger();
#endif
}
