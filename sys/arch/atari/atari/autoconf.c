/*	$NetBSD: autoconf.c,v 1.3 1996/01/06 20:10:41 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman
 * Copyright (c) 1994 Christian E. Hopps
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
#include <machine/disklabel.h>
#include <machine/cpu.h>

void configure __P((void));
static void setroot __P((void));
void swapconf __P((void));
void mbattach __P((struct device *, struct device *, void *));
int mbprint __P((void *, char *));
int mbmatch __P((struct device *, struct cfdata *, void *));

extern int cold;	/* 1 if still booting (locore.s) */
int atari_realconfig;
#include <sys/kernel.h>

/*
 * called at boot time, configure all devices on system
 */
void
configure()
{
	extern int atari_realconfig;
	
	atari_realconfig = 1;

	if (config_rootfound("mainbus", "mainbus") == 0)
		panic("no mainbus found");
	
#ifdef GENERIC
	if((boothowto & RB_ASKNAME) == 0)
		setroot();
	setconf();
#else
	setroot();
#endif
	swapconf();
	cold = 0;
}

/*ARGSUSED*/
int
simple_devprint(auxp, pnp)
	void *auxp;
	char *pnp;
{
	return(QUIET);
}

/*
 * use config_search to find appropriate device, then call that device
 * directly with NULL device variable storage.  A device can then 
 * always tell the difference between the real and console init 
 * by checking for NULL.
 */
int
atari_config_found(pcfp, pdp, auxp, pfn)
	struct cfdata *pcfp;
	struct device *pdp;
	void *auxp;
	cfprint_t pfn;
{
	struct device temp;
	struct cfdata *cf;
	extern int	atari_realconfig;

	if (atari_realconfig)
		return(config_found(pdp, auxp, pfn));

	if (pdp == NULL)
		pdp = &temp;

	pdp->dv_cfdata = pcfp;
	if ((cf = config_search((cfmatch_t)NULL, pdp, auxp)) != NULL) {
		cf->cf_driver->cd_attach(pdp, NULL, auxp);
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
int
config_console()
{	
	struct cfdata *cf;

	/*
	 * we need mainbus' cfdata.
	 */
	cf = config_rootsearch(NULL, "mainbus", "mainbus");
	if (cf == NULL)
		panic("no mainbus");
	atari_config_found(cf, NULL, "grfbus", NULL);
}

void
swapconf()
{
	struct swdevt	*swp;
	u_int		maj;
	int		nb;

	for (swp = swdevt; swp->sw_dev > 0; swp++) {
		maj = major(swp->sw_dev);

		if (maj > nblkdev)
			break;

		if (bdevsw[maj].d_psize) {
			nb = bdevsw[maj].d_psize(swp->sw_dev);
			if (nb > 0 && 
			    (swp->sw_nblks == 0 || swp->sw_nblks > nb))
				swp->sw_nblks = nb;
			else swp->sw_nblks = 0;
		}
		swp->sw_nblks = ctod(dtoc(swp->sw_nblks));
	}
	dumpconf();
	if( dumplo < 0)
		dumplo = 0;

}

#define	DOSWAP	/* change swdevt and dumpdev				*/
dev_t	bootdev = 0;

static	char devname[][2] = {
	0,0,
	0,0,
	'f','d',	/* 2 = fd */
	0,0,
	's','d',	/* 4 = sd -- SCSI system */
};

static void
setroot()
{
	int		majdev, mindev, unit, part, adaptor;
	dev_t		temp, orootdev;
	struct swdevt	*swp;

	if (boothowto & RB_DFLTROOT
		|| (bootdev & B_MAGICMASK) != (u_long)B_DEVMAGIC)
		return;
	majdev = (bootdev >> B_TYPESHIFT) & B_TYPEMASK;
	if(majdev > sizeof(devname) / sizeof(devname[0]))
		return;
	adaptor  = (bootdev >> B_ADAPTORSHIFT) & B_ADAPTORMASK;
	part     = (bootdev >> B_PARTITIONSHIFT) & B_PARTITIONMASK;
	unit     = (bootdev >> B_UNITSHIFT) & B_UNITMASK;
	orootdev = rootdev;
	rootdev  = MAKEDISKDEV(majdev, unit, part);
	/*
	 * If the original rootdev is the same as the one
	 * just calculated, don't need to adjust the swap configuration.
	 */
	if (rootdev == orootdev)
		return;
	printf("changing root device to %c%c%d%c\n",
		devname[majdev][0], devname[majdev][1],
		unit, part + 'a');
#ifdef DOSWAP
	mindev = DISKUNIT(rootdev);
	for (swp = swdevt; swp->sw_dev; swp++) {
		if (majdev == major(swp->sw_dev)
			&& mindev == DISKUNIT(swp->sw_dev)) {
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
 * mainbus driver 
 */
struct cfdriver mainbuscd = {
	NULL, "mainbus", (cfmatch_t)mbmatch, mbattach, 
	DV_DULL, sizeof(struct device), NULL, 0
};

int
mbmatch(pdp, cfp, auxp)
	struct device *pdp;
	struct cfdata *cfp;
	void *auxp;
{
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
	printf ("\n");
	config_found(dp, "nvr"    , simple_devprint);
	config_found(dp, "clock"  , simple_devprint);
	config_found(dp, "grfbus" , simple_devprint);
	config_found(dp, "kbd"    , simple_devprint);
	config_found(dp, "fdc"    , simple_devprint);
	config_found(dp, "zs"     , simple_devprint);
	config_found(dp, "ncrscsi", simple_devprint);
}

int
mbprint(auxp, pnp)
	void *auxp;
	char *pnp;
{
	if (pnp)
		printf("%s at %s", (char *)auxp, pnp);
	return(UNCONF);
}
