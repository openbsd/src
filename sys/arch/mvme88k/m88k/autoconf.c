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
 *
 *	$Id: autoconf.c,v 1.1.1.1 1995/10/18 10:54:25 deraadt Exp $
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <machine/cpu.h>

void configure __P((void));
void setroot __P((void));
void swapconf __P((void));

int realconfig=0;
int cold;	/* 1 if still booting */
#include <sys/kernel.h>
/*
 * called at boot time, configure all devices on system
 */
void
configure()
{
	/*
	 * this is the real thing baby (i.e. not console init)
	 */
	realconfig = 1;

	if (config_rootfound("mainbus", "mainbus") == 0)
		panic("no mainbus found");
	
#ifdef GENERIC
	if ((boothowto & RB_ASKNAME) == 0)
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
}

void
swapconf()
{
	struct swdevt *swp;
	u_int maj;
	int nb;

	for (swp = swdevt; swp->sw_dev > 0; swp++) {
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
	if (dumplo == 0 && dumpdev != NODEV && bdevsw[major(dumpdev)].d_psize)
	/*dumplo = (*bdevsw[major(dumpdev)].d_psize)(dumpdev) - physmem;*/
		dumplo = (*bdevsw[major(dumpdev)].d_psize)(dumpdev) -
			ctob(physmem)/DEV_BSIZE;
	if (dumplo < 0)
		dumplo = 0;

}

#define	DOSWAP			/* change swdevt and dumpdev */
u_long	bootdev = 0;		/* should be dev_t, but not until 32 bits */

static	char devname[][2] = {
	0,0,
	0,0,
	0,0,
	0,0,
	's','d',	/* 4 = sd -- new SCSI system */
};

void
setroot()
{
	int majdev, mindev, unit, part, adaptor;
	dev_t temp, orootdev;
	struct swdevt *swp;

	printf("setroot boothowto %x bootdev %x\n", boothowto, bootdev);
	if (boothowto & RB_DFLTROOT ||
	    (bootdev & B_MAGICMASK) != (u_long)B_DEVMAGIC)
		return;
	majdev = (bootdev >> B_TYPESHIFT) & B_TYPEMASK;
	if (majdev > sizeof(devname) / sizeof(devname[0]))
		return;
	adaptor = (bootdev >> B_ADAPTORSHIFT) & B_ADAPTORMASK;
	part = (bootdev >> B_PARTITIONSHIFT) & B_PARTITIONMASK;
	unit = (bootdev >> B_UNITSHIFT) & B_UNITMASK;
	orootdev = rootdev;
	rootdev = MAKEDISKDEV(majdev, unit, part);
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
		printf("DOSWAP swap %x dev %x\n", swp, swp->sw_dev);
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
