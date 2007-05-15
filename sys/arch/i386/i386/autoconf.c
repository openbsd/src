/*	$OpenBSD: autoconf.c,v 1.74 2007/05/15 01:56:47 deraadt Exp $	*/
/*	$NetBSD: autoconf.c,v 1.20 1996/05/03 19:41:56 christos Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)autoconf.c	7.1 (Berkeley) 5/9/91
 */

/*
 * Setup the system to run on the current machine.
 *
 * cpu_configure() is called at boot time and initializes the vba
 * device tables and the memory controller monitoring.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <uvm/uvm_extern.h>

#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/gdt.h>
#include <machine/biosvar.h>
#include <machine/kvm86.h>

#include <dev/cons.h>

#include "ioapic.h"

#if NIOAPIC > 0
#include <machine/i82093var.h>
#endif

void diskconf(void);

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
extern dev_t bootdev;

/* Support for VIA C3 RNG */
#ifdef I686_CPU
extern struct timeout viac3_rnd_tmo;
extern int	viac3_rnd_present;
void		viac3_rnd(void *);

#ifdef CRYPTO
void		viac3_crypto_setup(void);
extern int	i386_has_xcrypt;
#endif /* CRYPTO */
#endif

/*
 * Determine i/o configuration for a machine.
 */
void
cpu_configure(void)
{
	/*
	 * Note, on i386, configure is not running under splhigh unlike other
	 * architectures.  This fact is used by the pcmcia irq line probing.
	 */

	startrtclock();

	gdt_init();		/* XXX - pcibios uses gdt stuff */

	/* Set up proc0's TSS and LDT */
	i386_proc0_tss_ldt_init();

#ifdef KVM86
	kvm86_init();
#endif

	if (config_rootfound("mainbus", NULL) == NULL)
		panic("cpu_configure: mainbus not configured");

#if NIOAPIC > 0
	if (nioapics > 0)
		goto nomasks;
#endif
	printf("biomask %x netmask %x ttymask %x\n", (u_short)IMASK(IPL_BIO),
	    (u_short)IMASK(IPL_NET), (u_short)IMASK(IPL_TTY));

#if NIOAPIC > 0
 nomasks:
	ioapic_enable();
#endif

	proc0.p_addr->u_pcb.pcb_cr0 = rcr0();

#ifdef MULTIPROCESSOR
	/* propagate TSS and LDT configuration to the idle pcb's. */
	cpu_init_idle_pcbs();
#endif
	spl0();

	/*
	 * We can not know which is our root disk, defer
	 * until we can checksum blocks to figure it out.
	 */
	md_diskconf = diskconf;
	cold = 0;

#ifdef I686_CPU
	/*
	 * At this point the RNG is running, and if FSXR is set we can
	 * use it.  Here we setup a periodic timeout to collect the data.
	 */
	if (viac3_rnd_present) {
		timeout_set(&viac3_rnd_tmo, viac3_rnd, &viac3_rnd_tmo);
		viac3_rnd(&viac3_rnd_tmo);
	}
#ifdef CRYPTO
	/*
	 * Also, if the chip has crypto available, enable it.
	 */
	if (i386_has_xcrypt)
		viac3_crypto_setup();
#endif /* CRYPTO */
#endif
}

void
device_register(struct device *dev, void *aux)
{
}

/*
 * Now that we are fully operational, we can checksum the
 * disks, and using some heuristics, hopefully are able to
 * always determine the correct root disk.
 */
void
diskconf(void)
{
	int majdev, unit, part = 0;
	struct device *bootdv = NULL;
	dev_t tmpdev;
	char buf[128];
#if defined(NFSCLIENT)
	extern bios_bootmac_t *bios_bootmac;
#endif

	dkcsumattach();

	if ((bootdev & B_MAGICMASK) == (u_int)B_DEVMAGIC) {
		majdev = B_TYPE(bootdev);
		unit = B_UNIT(bootdev);
		part = B_PARTITION(bootdev);
		snprintf(buf, sizeof buf, "%s%d%c", findblkname(majdev),
		    unit, part + 'a');
		bootdv = parsedisk(buf, strlen(buf), part, &tmpdev);
	}

#if defined(NFSCLIENT)
	if (bios_bootmac) {
		struct ifnet *ifp;

		printf("PXE boot MAC address %s, ",
		    ether_sprintf(bios_bootmac->mac));
		for (ifp = TAILQ_FIRST(&ifnet); ifp != NULL;
		    ifp = TAILQ_NEXT(ifp, if_list)) {
			if ((ifp->if_type == IFT_ETHER ||
			    ifp->if_type == IFT_FDDI) &&
			    bcmp(bios_bootmac->mac,
			    ((struct arpcom *)ifp)->ac_enaddr,
			    ETHER_ADDR_LEN) == 0)
				break;
		}
		if (ifp) {
			printf("interface %s\n", ifp->if_xname);
			mountroot = nfs_mountroot;	/* potentially */
			bootdv = parsedisk(ifp->if_xname, strlen(ifp->if_xname),
			    0, &tmpdev);
			part = 0;
		} else
			printf("unknown interface\n");
	}
#endif

	setroot(bootdv, part, RB_USERREQ);
	dumpconf();
}

struct nam2blk nam2blk[] = {
	{ "wd",		0 },
	{ "fd",		2 },
	{ "sd",		4 },
	{ "cd",		6 },
	{ "mcd",	7 },
	{ "rd",		17 },
	{ "raid",	19 },
	{ NULL,		-1 }
};
