/* $NetBSD: autoconf.c,v 1.2 1996/03/06 23:11:36 mark Exp $ */

/*
 * Copyright (c) 1994,1995 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * autoconf.c
 *
 * Autoconfiguration functions
 *
 * Created      : 08/10/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <machine/irqhandler.h>
#include <machine/bootconfig.h>

#include "wdc.h"
#include "fdc.h"
#include "rd.h"
#include "sd.h"
#include "cd.h"
#include "wcd.h"

extern dev_t rootdev;
extern dev_t swapdev;
extern dev_t dumpdev;
extern dev_t argdev;

extern struct swdevt swdevt[];

extern char *boot_args;
extern int pmap_debug_level;

char *	strstr	__P((char */*s1*/, char */*s2*/));
long	strtoul __P((const char *, char **, int));

/* Table major numbers for the device names, NULL terminated */

struct {
    char *name;
    dev_t dev;
} rootdevices[] = {
#if NWDC > 0
	{ "wd", 0x10 },
#endif
#if NFDC > 0
	{ "fd", 0x11 },
#endif
#if NRD > 0
	{ "rd", 0x12 },
#endif
#if NSD > 0
	{ "sd", 0x18 },
#endif
#if NCD > 0
	{ "cd", 0x1a },
#endif
#if NWCD > 0
	{ "wcd", 0x14 },
#endif
	{ NULL, 0x00 },
};

/* Decode a device name to a major and minor number */

dev_t
get_device(name)
	char *name;
{
	int loop;
	int unit;
	int part;
    
	if (strncmp(name, "/dev/", 5) == 0)
		name += 5;

	for (loop = 0; rootdevices[loop].name; ++loop) {
		if (strncmp(name, rootdevices[loop].name,
		    strlen(rootdevices[loop].name)) == 0) {
			name += strlen(rootdevices[loop].name);
			unit = name[0] - '0';
			part = name[1] - 'a';
			if (unit < 0 || unit > 9)
				return(NODEV);
			if (part < 0 || part > MAXPARTITIONS)
				return(NODEV);
			return(makedev(rootdevices[loop].dev,
			    unit * MAXPARTITIONS + part));
		}
	} 
	return(NODEV);  
}


/* Set the rootdev variable from the root specifier in the boot args */

void
set_root_device()
{
	char *ptr;
            
	if (boot_args) {
		ptr = strstr(boot_args, "root=");
		if (ptr) {
			ptr += 5;
			rootdev = get_device(ptr);
              
			if (pmap_debug_level >= 0)
				printf("rootdev = %08x\n", rootdev);
		}
	}

	if (rootdev == NODEV)
		panic("No root device specified in boot config\n");
}


/* Set the swap devices from the swap specifiers in the boot ars */

void
set_swap_device()
{
	char *ptr;
	int nswap = 0;
            
	if (boot_args) {
		ptr = boot_args;
		do {
			ptr = strstr(ptr, "swap=");
			if (ptr) {
				ptr += 5;
				swdevt[nswap].sw_dev = get_device(ptr);

				/*
				 * Remember the first swap device
				 */

				if (nswap == 0)
					swapdev = get_device(ptr);
				++nswap;
			}
		} while (ptr);
	}
}


/*
 * Configure swap space and related parameters.
 */

void
swapconf()
{
	register struct swdevt *swp;
	register int nblks;
	int swapsize = -1;
	char *ptr;
	int maj;
	int s;		/* The spl stuff was here for debugging reaons */

	/*
	 * Loop round all the defined swap device configuring them.
	 */

	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		maj = major(swp->sw_dev);
		if (maj > nblkdev)
			break;
		if (bdevsw[maj].d_psize) {
			s = spltty();
			printf("swap dev %04x ", swp->sw_dev);
			(void)splx(s);
			if (swapsize == -1)
				nblks = (*bdevsw[maj].d_psize)(swp->sw_dev);
  		  	else
				nblks = swapsize;
			s = spltty();
			if (nblks == -1)
				printf("-> device not configured for swap\n");
			else
				printf("-> %d bytes\n", nblks*DEV_BSIZE);
			(void)splx(s);
			if (nblks != -1 &&
			    (swp->sw_nblks == 0 || swp->sw_nblks > nblks))
				swp->sw_nblks = nblks;
			swp->sw_nblks = ctod(dtoc(swp->sw_nblks));
		}
	}
}


/* Set up the root and swap device numbers, configure the swap space and dump space */

void
set_boot_devs()
{
	set_root_device();
	set_swap_device();

	if (swapdev == NODEV && minor(rootdev) < (MAXPARTITIONS - 2))
		swapdev = makedev(major(rootdev), minor(rootdev) + 1);

	dumpdev = swapdev;
	argdev = swapdev;
	swdevt[0].sw_dev = swapdev;

	swapconf();
	dumpconf();
}


/*
 * void configure()
 *
 * Configure all the root devices
 * The root devices are expected to configure their own children
 */

void
configure()
{

/*
 * Loop round all the root devices configuring them. Configure failure
 * is not expected for the root devices
 */

	config_rootfound("mainbus", NULL);
	config_rootfound("podulebus", NULL);

/* Debugging information */

	printf("ipl_bio=%08x ipl_net=%08x ipl_tty=%08x ipl_clock=%08x ipl_imp=%08x\n",
	    irqmasks[IPL_BIO], irqmasks[IPL_NET], irqmasks[IPL_TTY],
	    irqmasks[IPL_CLOCK], irqmasks[IPL_IMP]);

/* Time to start taking interrupts so lets open the flood gates .... */
         
	(void)spl0();
}

/* End of autoconf.c */
