/*	$OpenBSD: autoconf.c,v 1.12 1998/04/09 07:34:00 niklas Exp $	*/
/*	$NetBSD: autoconf.c,v 1.59 1998/01/15 21:55:51 is Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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
 *      This product includes software developed by Christian E. Hopps.
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <machine/cpu.h>
#include <amiga/amiga/cfdev.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/custom.h>

void setroot __P((void));
void swapconf __P((void));
void mbattach __P((struct device *, struct device *, void *));
int mbprint __P((void *, const char *));
int mbmatch __P((struct device *, void *, void *));

int cold;	/* 1 if still booting */
#include <sys/kernel.h>
/*
 * called at boot time, configure all devices on system
 */
void
configure()
{
	int s;

	/*
	 * this is the real thing baby (i.e. not console init)
	 */
	amiga_realconfig = 1;
#ifdef DRACO
	if (is_draco()) {
		*draco_intena &= ~DRIRQ_GLOBAL;
	} else
#endif
	custom.intena = INTF_INTEN;
	s = splhigh();

	if (config_rootfound("mainbus", "mainbus") == NULL)
		panic("no mainbus found");
#ifdef DEBUG_KERNEL_START
	printf("survived autoconf, going to enable interrupts\n");
#endif
	
#ifdef DRACO
	if (is_draco()) {
		*draco_intena |= DRIRQ_GLOBAL;
		/* softints always enabled */
	} else
#endif
	{
		custom.intena = INTF_SETCLR | INTF_INTEN;

		/* also enable hardware aided software interrupts */
		custom.intena = INTF_SETCLR | INTF_SOFTINT;
	}
	splx(s);
#ifdef DEBUG_KERNEL_START
	printf("survived interrupt enable\n");
#endif

#ifdef GENERIC
	if ((boothowto & RB_ASKNAME) == 0) {
		setroot();
#ifdef DEBUG_KERNEL_START
		printf("survived setroot()\n");
#endif
	}
	setconf();
#ifdef DEBUG_KERNEL_START
	printf("survived setconf()\n");
#endif
#else
	setroot();
#ifdef DEBUG_KERNEL_START
	printf("survived setroot()\n");
#endif
#endif
#ifdef DEBUG_KERNEL_START
	printf("survived root device search\n");
#endif
	swapconf();
#ifdef DEBUG_KERNEL_START
	printf("survived swap device search\n");
#endif
	dumpconf();
	if (dumplo < 0)
		dumplo = 0;
#ifdef DEBUG_KERNEL_START
	printf("survived dump device search\n");
#endif
	cold = 0;
}

/*ARGSUSED*/
int
simple_devprint(auxp, pnp)
	void *auxp;
	const char *pnp;
{
	return(QUIET);
}

int
matchname(fp, sp)
	char *fp, *sp;
{
	int len;

	len = strlen(fp);
	if (strlen(sp) != len)
		return(0);
	if (bcmp(fp, sp, len) == 0)
		return(1);
	return(0);
}

/*
 * use config_search to find appropriate device, then call that device
 * directly with NULL device variable storage.  A device can then 
 * always tell the difference betwean the real and console init 
 * by checking for NULL.
 */
int
amiga_config_found(pcfp, pdp, auxp, pfn)
	struct cfdata *pcfp;
	struct device *pdp;
	void *auxp;
	cfprint_t pfn;
{
	struct device temp;
	struct cfdata *cf;

	if (amiga_realconfig)
		return(config_found(pdp, auxp, pfn) != NULL);

	if (pdp == NULL)
		pdp = &temp;

	pdp->dv_cfdata = pcfp;
	if ((cf = config_search((cfmatch_t)NULL, pdp, auxp)) != NULL) {
		cf->cf_attach->ca_attach(pdp, NULL, auxp);
		pdp->dv_cfdata = NULL;
		return(1);
	}
	pdp->dv_cfdata = NULL;
	return(0);
}

/*
 * this function needs to get enough configured to do a console
 * basically this means start attaching the grfxx's that support 
 * the console. Kinda hacky but it works.
 */
void
config_console()
{	
	struct cfdata *cf;

	/*
	 * we need mainbus' cfdata.
	 */
	cf = config_rootsearch(NULL, "mainbus", "mainbus");
	if (cf == NULL) {
		panic("no mainbus");
	}

	/*
	 * delay clock calibration.
	 */
	amiga_config_found(cf, NULL, "clock", NULL);

	/*
	 * internal grf.
	 */
#ifdef DRACO
	if (!(is_draco()))
#endif
		amiga_config_found(cf, NULL, "grfcc", NULL);
	/*
	 * zbus knows when its not for real and will
	 * only configure the appropriate hardware
	 */
	amiga_config_found(cf, NULL, "zbus", NULL);
}

/* 
 * mainbus driver 
 */
struct cfattach mainbus_ca = {
	sizeof(struct device), mbmatch, mbattach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL, NULL, 0
};

int
mbmatch(pdp, match, auxp)
	struct device *pdp;
	void *match, *auxp;
{
	struct cfdata *cfp = match;

	if (cfp->cf_unit > 0)
		return(0);
	/*
	 * We are always here
	 */
	return(1);
}

/*
 * "find" all the things that should be there.
 */
void
mbattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	printf("\n");
	config_found(dp, "clock", simple_devprint);
	if (is_a3000() || is_a4000()) {
		config_found(dp, "a34kbbc", simple_devprint);
	} else
#ifdef DRACO
	if (!is_draco())
#endif
	{
		config_found(dp, "a2kbbc", simple_devprint);
	}
#ifdef DRACO
	if (is_draco()) {
		config_found(dp, "drbbc", simple_devprint);
		config_found(dp, "kbd", simple_devprint);
		config_found(dp, "drsc", simple_devprint);
		config_found(dp, "drsupio", simple_devprint);
	} else 
#endif
	{
		config_found(dp, "ser", simple_devprint);
		config_found(dp, "par", simple_devprint);
		config_found(dp, "kbd", simple_devprint);
		config_found(dp, "ms", simple_devprint);
		config_found(dp, "ms", simple_devprint);
		config_found(dp, "grfcc", simple_devprint);
		config_found(dp, "fdc", simple_devprint);
	}
	if (is_a4000() || is_a1200())
		config_found(dp, "idesc", simple_devprint);
	if (is_a4000())			/* Try to configure A4000T SCSI */
		config_found(dp, "afsc", simple_devprint);
	if (is_a3000())
		config_found(dp, "ahsc", simple_devprint);
#ifdef DRACO
	if (!is_draco())
#endif
		config_found(dp, "aucc", simple_devprint);

	config_found(dp, "zbus", simple_devprint);
}

int
mbprint(auxp, pnp)
	void *auxp;
	const char *pnp;
{
	if (pnp)
		printf("%s at %s", (char *)auxp, pnp);
	return(UNCONF);
}

void
swapconf()
{
	struct swdevt *swp;
	u_int maj;
	int nb;

	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		maj = major(swp->sw_dev);

		if (maj > nblkdev)
			break;

		if (bdevsw[maj].d_psize) {
			nb = bdevsw[maj].d_psize(swp->sw_dev);
			if (nb > 0 && 
			    (swp->sw_nblks == 0 || swp->sw_nblks > nb))
				swp->sw_nblks = nb;
			else
				swp->sw_nblks = 0;
		}
		swp->sw_nblks = ctod(dtoc(swp->sw_nblks));
	}
}

#define	DOSWAP			/* change swdevt and dumpdev */
u_long	bootdev = 0;		/* should be dev_t, but not until 32 bits */

static	char devname[][2] = {
	{ 0	,0	},
	{ 0	,0	},
	{ 'f'	,'d'	},	/* 2 = fd */
	{ 0	,0	},
	{ 's'	,'d'	}	/* 4 = sd -- new SCSI system */
};

void
setroot()
{
	int majdev, mindev, unit, part, adaptor;
	dev_t temp = 0;
	dev_t orootdev;
	struct swdevt *swp;

	if (boothowto & RB_DFLTROOT ||
	    (bootdev & B_MAGICMASK) != (u_long)B_DEVMAGIC)
		return;
	majdev = B_TYPE(bootdev);
	if (majdev > sizeof(devname) / sizeof(devname[0]))
		return;
	adaptor = B_ADAPTOR(bootdev);
	part = B_PARTITION(bootdev);
	unit = B_UNIT(bootdev);
	orootdev = rootdev;
	rootdev = MAKEDISKDEV(majdev, unit, part);
	/*
	 * If the original rootdev is the same as the one
	 * just calculated, don't need to adjust the swap configuration.
	 */
	if (rootdev == orootdev)
		return;
	printf("changing root device to %c%c%d%c\n", devname[majdev][0],
	    devname[majdev][1], unit, part + 'a');
#ifdef DOSWAP
	mindev = DISKUNIT(rootdev);
	for (swp = swdevt; swp->sw_dev; swp++) {
		if (majdev == major(swp->sw_dev) &&
		    mindev == DISKUNIT(swp->sw_dev)) {
			temp = swdevt[0].sw_dev;
			swdevt[0].sw_dev = swp->sw_dev;
			swp->sw_dev = temp;
			break;
		}
	}
	if (swp->sw_dev == 0)
		return;
	/*
	 * If dumpdev was the same as the old primary swap
	 * device, move it to the new primary swap device.
	 */
	if (temp == dumpdev)
		dumpdev = swdevt[0].sw_dev;
#endif
}


/*
 * Try to determine, of this machine is an A3000, which has a builtin
 * realtime clock and scsi controller, so that this hardware is only
 * included as "configured" if this IS an A3000
 */

int a3000_flag = 1;		/* patchable */
#ifdef A4000
int a4000_flag = 1;		/* patchable - default to A4000 */
#else
int a4000_flag = 0;		/* patchable */
#endif

int
is_a3000()
{
	/* this is a dirty kludge.. but how do you do this RIGHT ? :-) */
	extern long boot_fphystart;
	short sc;

	if ((machineid >> 16) == 3000)
		return (1);			/* It's an A3000 */
	if (machineid >> 16)
		return (0);			/* It's not an A3000 */
	/* Machine type is unknown, so try to guess it */
	/* where is fastram on the A4000 ?? */
	/* if fastram is below 0x07000000, assume it's not an A3000 */
	if (boot_fphystart < 0x07000000)
		return(0);
	/*
	 * OK, fastram starts at or above 0x07000000, check specific
	 * machines
	 */
	for (sc = 0; sc < ncfdev; sc++) {
		switch (cfdev[sc].rom.manid) {
		case 2026:		/* Progressive Peripherals, Inc */
			switch (cfdev[sc].rom.prodid) {
			case 0:		/* PPI Mercury - A3000 */
			case 1:		/* PP&S A3000 '040 */
				return(1);
			case 150:	/* PPI Zeus - it's an A2000 */
			case 105:	/* PP&S A2000 '040 */
			case 187:	/* PP&S A500 '040 */
				return(0);
			}
			break;

		case 2112:			/* IVS */
			switch (cfdev[sc].rom.prodid) {
			case 242:
				return(0);	/* A2000 accelerator? */
			}
			break;
		}
	}
	return (a3000_flag);		/* XXX let flag tell now */
}

int
is_a4000()
{
	if ((machineid >> 16) == 4000)
		return (1);		/* It's an A4000 */
	if ((machineid >> 16) == 1200)
		return (0);		/* It's an A1200, so not A4000 */
#ifdef DRACO
	if (is_draco())
		return (0);
#endif
	/* Do I need this any more? */
	if ((custom.deniseid & 0xff) == 0xf8)
		return (1);
#ifdef DEBUG
	if (a4000_flag)
		printf("Denise ID = %04x\n", (unsigned short)custom.deniseid);
#endif
	if (machineid >> 16)
		return (0);		/* It's not an A4000 */
	return (a4000_flag);		/* Machine type not set */
}

int
is_a1200()
{
	if ((machineid >> 16) == 1200)
		return (1);		/* It's an A1200 */
	return (0);			/* Machine type not set */
}

#ifdef DRACO
int
is_draco()
{
	if ((machineid >> 24) == 0x7D)
		return ((machineid >> 16) & 0xFF);
	return (0);
}
#endif
