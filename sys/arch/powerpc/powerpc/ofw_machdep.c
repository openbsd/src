/*	$OpenBSD: ofw_machdep.c,v 1.8 1998/09/20 22:11:48 rahnds Exp $	*/
/*	$NetBSD: ofw_machdep.c,v 1.1 1996/09/30 16:34:50 ws Exp $	*/

/*
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/systm.h>

#include <machine/powerpc.h>
#include <machine/autoconf.h>

void OF_exit __P((void)) __attribute__((__noreturn__));
void OF_boot __P((char *bootspec)) __attribute__((__noreturn__));

#define	OFMEM_REGIONS	32
static struct mem_region OFmem[OFMEM_REGIONS + 1], OFavail[OFMEM_REGIONS + 3];

/*
 * This is called during initppc, before the system is really initialized.
 * It shall provide the total and the available regions of RAM.
 * Both lists must have a zero-size entry as terminator.
 * The available regions need not take the kernel into account, but needs
 * to provide space for two additional entry beyond the terminating one.
 */
void
mem_regions(memp, availp)
	struct mem_region **memp, **availp;
{
	int phandle, i, j, cnt;
	
	/*
	 * Get memory.
	 */
	if ((phandle = OF_finddevice("/memory")) == -1
	    || OF_getprop(phandle, "reg",
			  OFmem, sizeof OFmem[0] * OFMEM_REGIONS)
	       <= 0
	    || OF_getprop(phandle, "available",
			  OFavail, sizeof OFavail[0] * OFMEM_REGIONS)
	       <= 0)
		panic("no memory?");
	*memp = OFmem;
	*availp = OFavail;
}

void
ppc_exit()
{
	OF_exit();
}

void
ppc_boot(str)
	char *str;
{
	OF_boot(str);
}

#include <dev/ofw/openfirm.h>

typedef void  (void_f) (void);
extern void_f *pending_int_f;
void ofw_do_pending_int();
extern int system_type;

void ofw_intr_init();
void
ofrootfound()
{
	int node;
	struct ofprobe probe;
		
	if (!(node = OF_peer(0)))
		panic("No PROM root");
	probe.phandle = node;
	if (!config_rootfound("ofroot", &probe))
		panic("ofroot not configured");
	if (system_type == OFWMACH) {
		pending_int_f = ofw_do_pending_int;
		ofw_intr_init();
	}
}
void
ofw_intr_establish()
{
	if (system_type == OFWMACH) {
		pending_int_f = ofw_do_pending_int;
		ofw_intr_init();
	}
}
void
ofw_intr_init()
{
	/*
	 * There are tty, network and disk drivers that use free() at interrupt
	 * time, so imp > (tty | net | bio).
	 */
	/* with openfirmware drivers all levels block clock
	 * (have to block polling)
	 */
	imask[IPL_IMP] = SPL_CLOCK;
	imask[IPL_TTY] = SPL_CLOCK | SINT_TTY;
	imask[IPL_NET] = SPL_CLOCK | SINT_NET;
	imask[IPL_BIO] = SPL_CLOCK;
	imask[IPL_IMP] |= imask[IPL_TTY] | imask[IPL_NET] | imask[IPL_BIO];

	/*
	 * Enforce a hierarchy that gives slow devices a better chance at not
	 * dropping data.
	 */
	imask[IPL_TTY] |= imask[IPL_NET] | imask[IPL_BIO];
	imask[IPL_NET] |= imask[IPL_BIO];

	/*
	 * These are pseudo-levels.
	 */
	imask[IPL_NONE] = 0x00000000;
	imask[IPL_HIGH] = 0xffffffff;

}
void
ofw_do_pending_int()
{
	struct intrhand *ih;
	int vector;
	int pcpl;
	int hwpend;
	int emsr, dmsr;
static int processing;

	if(processing)
		return;

	processing = 1;
	__asm__ volatile("mfmsr %0" : "=r"(emsr));
	dmsr = emsr & ~PSL_EE;
	__asm__ volatile("mtmsr %0" :: "r"(dmsr));


	pcpl = splhigh();		/* Turn off all */
	if((ipending & SINT_CLOCK) && (pcpl & imask[IPL_CLOCK] == 0)) {
		ipending &= ~SINT_CLOCK;
		softclock();
	}
	if((ipending & SINT_NET) && ((pcpl & imask[IPL_NET]) == 0) ) {
		extern int netisr;
		int pisr = netisr;
		netisr = 0;
		ipending &= ~SINT_NET;
		softnet(pisr);
	}
	ipending &= pcpl;
	cpl = pcpl;	/* Don't use splx... we are here already! */
	__asm__ volatile("mtmsr %0" :: "r"(emsr));
	processing = 0;
}
u_int32_t ppc_console_iomem=0;
u_int32_t ppc_console_addr=0;
u_int32_t ppc_console_qhandle=0;
u_int32_t ppc_console_serfreq;

void
ofwtrysercon(char *name, int qhandle)
{
/* for serial we want regs field */
	int regs[4];
	int freq;
	int regn;
	if ((regn = OF_getprop(qhandle, "reg", &regs[0], sizeof regs)) >= 0) {

		if (regs[1] == 0x3f8) {
			/* found preferred console */
			ppc_console_addr = regs[1];
			ppc_console_qhandle = qhandle;
			ppc_console_iomem=0; /* 0 means io, 1 means mem */
		}
		if ((regs[1] == 0x2e8) && (ppc_console_addr == 0)) {
			/* found nonpreferred console */
			ppc_console_addr = regs[1];
			ppc_console_qhandle = qhandle;
			ppc_console_iomem=0; /* 0 means io, 1 means mem */
		}
	}
	if ((OF_getprop(qhandle, "clock-frequency", &freq, sizeof regs)) >= 0) {
		/* MCG value for this does not agree with PC value,
		 * but works correctly (while PC value does not),
		 * does VI set this correctly???
		 */
		ppc_console_serfreq=freq;
	}
}
#include <machine/bat.h>
/* HACK */
#include <powerpc/pci/mpc106reg.h>
void
ofwconprobe()
{
	int qhandle, phandle;
	char name[32];
	for (qhandle = OF_peer(0); qhandle; qhandle = phandle) {
		if (OF_getprop(qhandle, "device_type", name, sizeof name) >= 0)
		{
			if (strcmp (name, "serial") == 0) {
				ofwtrysercon (name, qhandle);
			}
		}

		if (phandle = OF_child(qhandle))
			continue;
		while (qhandle) {
			if (phandle = OF_peer(qhandle))
				break;
			qhandle = OF_parent(qhandle);
		}
	}
	/* setup pci/isa as necessary to found map io area */
#if 0
	printf("found desired console address %x qhandle %x serfreq %x\n",
		ppc_console_addr, ppc_console_qhandle,
		ppc_console_serfreq);
#endif
		
	/* do this from probed values, not from constants */
	addbatmap(MPC106_V_ISA_IO_SPACE, MPC106_P_ISA_IO_SPACE, BAT_I); 
	addbatmap(0xB0000000, 0xB0000000, BAT_I);  /* map interrupt vector */  
}
