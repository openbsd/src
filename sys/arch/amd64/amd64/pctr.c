/*	$OpenBSD: pctr.c,v 1.1 2007/09/12 18:18:27 deraadt Exp $	*/

/*
 * Copyright (c) 2007 Mike Belopuhov
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Pentium performance counter driver for OpenBSD.
 * Copyright 1996 David Mazieres <dm@lcs.mit.edu>.
 *
 * Modification and redistribution in source and binary forms is
 * permitted provided that due credit is given to the author and the
 * OpenBSD project by leaving this copyright notice intact.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/systm.h>

#include <machine/psl.h>
#include <machine/pctr.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/specialreg.h>

/* Check for Model Specific Registers and RDMSR/WRMSR support */
#define usepctr		(cpu_feature & CPUID_MSR)

u_int64_t		pctr_idlcnt;	/* Gets incremented in locore.S */

int			pctr_isamd;
int			pctr_isintel;

static void		pctrrd(struct pctrst *);

static void
pctrrd(struct pctrst *st)
{
	int i, num, reg;

	num = pctr_isamd ? PCTR_AMD_NUM : PCTR_INTEL_NUM;
	reg = pctr_isamd ? MSR_K7_EVNTSEL0 : MSR_EVNTSEL0;

	for (i = 0; i < num; i++)
		st->pctr_fn[i] = rdmsr(reg + i);

	reg = pctr_isamd ? MSR_K7_PERFCTR0 : MSR_PERFCTR0;

	__asm __volatile("cli");

	st->pctr_tsc = rdtsc();

	for (i = 0; i < num; i++)
		st->pctr_hwc[i] = rdmsr(reg + i);
		/*st->pctr_hwc[i] = rdpmc(i);*/

	__asm __volatile("sti");
}

void
pctrattach(int num)
{

	if (num > 1)
		return;

	pctr_isamd = (strcmp(cpu_vendor, "AuthenticAMD") == 0);
	if (!pctr_isamd)
		pctr_isintel = (strcmp(cpu_vendor, "GenuineIntel") == 0);
	if (!pctr_isintel && !pctr_isamd)
		return;

	/* Enable RDTSC and RDPMC instructions from user-level. */
	if (usepctr) {
		__asm __volatile("movq %%cr4,%%rax\n"
				 "\tandq %0,%%rax\n"
				 "\torq %1,%%rax\n"
				 "\tmovq %%rax,%%cr4"
				 :: "i" (~CR4_TSD), "i" (CR4_PCE) : "rax");
		printf("pctr: user-level performance counters enabled\n");
	}
}

int
pctropen(dev_t dev, int oflags, int devtype, struct proc *p)
{

	if (minor(dev))
		return (ENXIO);
	return (0);
}

int
pctrclose(dev_t dev, int oflags, int devtype, struct proc *p)
{

	return (0);
}

int
pctrsel(int fflag, u_int32_t cmd, u_int32_t fn)
{
	int msrsel, msrval;

	cmd -= PCIOCS0;
	if (pctr_isamd) {
		if (cmd > PCTR_AMD_NUM-1)
			return (EINVAL);
		msrsel = MSR_K7_EVNTSEL0 + cmd;
		msrval = MSR_K7_PERFCTR0 + cmd;
	} else {
		if (cmd > PCTR_INTEL_NUM-1)
			return (EINVAL);
		msrsel = MSR_EVNTSEL0 + cmd;
		msrval = MSR_PERFCTR0 + cmd;
	}

	if (!(fflag & FWRITE))
		return (EPERM);
	if (fn & 0x380000)
		return (EINVAL);

	wrmsr(msrval, 0);
	wrmsr(msrsel, fn);
	wrmsr(msrval, 0);

	return (0);
}

int
pctrioctl(dev_t dev, u_int64_t cmd, caddr_t data, int fflag, struct proc *p)
{

	switch (cmd) {
	case PCIOCRD:
	{
		struct pctrst *st = (struct pctrst *)data;
		
		if (usepctr)
			pctrrd(st);
		st->pctr_idl = pctr_idlcnt;
		return (0);
	}
	case PCIOCS0:
	case PCIOCS1:
	case PCIOCS2:
	case PCIOCS3:
		if (usepctr)
			return (pctrsel(fflag, cmd, *(u_int32_t *)data));
		return (ENODEV);
	default:
		return (EINVAL);
	}
}
