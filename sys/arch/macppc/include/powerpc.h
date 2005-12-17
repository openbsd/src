/*	$OpenBSD: powerpc.h,v 1.8 2005/12/17 07:31:26 miod Exp $	*/
/*	$NetBSD: powerpc.h,v 1.1 1996/09/30 16:34:30 ws Exp $	*/

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
#ifndef	_MACHINE_POWERPC_H_
#define	_MACHINE_POWERPC_H_

struct mem_region {
	vaddr_t start;
	vsize_t size;
};

void mem_regions(struct mem_region **, struct mem_region **);

/*
 * These two functions get used solely in boot() in machdep.c.
 *
 * Not sure whether boot itself should be implementation dependent instead.	XXX
 */
typedef void (exit_f)(void) /*__attribute__((__noreturn__))*/ ;
typedef void (boot_f)(char *bootspec) /* __attribute__((__noreturn__))*/ ;
typedef void (vmon_f)(void);

/* firmware interface.
 * regardless of type of firmware used several items
 * are need from firmware to boot up.
 * these include:
 *	memory information
 *	vmsetup for firmware calls.
 *	default character print mechanism ???
 *	firmware exit (return)
 *	firmware boot (reset)
 *	vmon - tell firmware the bsd vm is active.
 */

typedef void (mem_regions_f)(struct mem_region **memp,
	struct mem_region **availp);

struct firmware {
	mem_regions_f	*mem_regions;
	exit_f		*exit;
	boot_f		*boot;
	vmon_f		*vmon;
	
#ifdef FW_HAS_PUTC
	boot_f		*putc;
#endif
};
extern  struct firmware *fw;
int ppc_open_pci_bridge(void);
void ppc_close_pci_bridge(int);
void install_extint(void (*handler) (void));
void ppc_intr_enable(int enable);
int ppc_intr_disable(void);

struct dumpmem {
	vaddr_t         start;
	vsize_t         end;
};
extern struct dumpmem dumpmem[VM_PHYSSEG_MAX];
extern u_int ndumpmem;
extern vaddr_t dumpspace;
#endif	/* _MACHINE_POWERPC_H_ */
