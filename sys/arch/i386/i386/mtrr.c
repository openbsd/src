/* $OpenBSD: mtrr.c,v 1.6 2002/03/14 01:26:32 millert Exp $ */
/*-
 * Copyright (c) 1999 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 1999 Brian Fundakowski Feldman
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/memrange.h>
#include <sys/systm.h>

#include <machine/specialreg.h>

/* Pull in the cpuid values from locore.s */
extern int cpu_id;
extern int cpu_feature;
extern char cpu_vendor[];

extern struct mem_range_ops i686_mrops;
extern struct mem_range_ops k6_mrops;

void mtrrattach(int);

void
mtrrattach (num)
	int num;
{
	if (num > 1)
		return;

	if (strcmp(cpu_vendor, "AuthenticAMD") == 0 &&
	    (cpu_id & 0xf00) == 0x500 &&
	    ((cpu_id & 0xf0) > 0x80 ||
	     ((cpu_id & 0xf0) == 0x80 &&
	      (cpu_id & 0xf) > 0x7))) {
		mem_range_softc.mr_op = &k6_mrops;
		
		/* Try for i686 MTRRs */
	} else if ((cpu_feature & CPUID_MTRR) &&
		   ((cpu_id & 0xf00) == 0x600) &&
		   ((strcmp(cpu_vendor, "GenuineIntel") == 0) ||
		    (strcmp(cpu_vendor, "AuthenticAMD") == 0))) {
		mem_range_softc.mr_op = &i686_mrops;
	    
	} 
	/* Initialise memory range handling */
	if (mem_range_softc.mr_op != NULL)
		mem_range_softc.mr_op->init(&mem_range_softc);
}


