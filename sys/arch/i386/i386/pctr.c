/*	$OpenBSD: pctr.c,v 1.18 2003/09/29 17:33:01 mickey Exp $	*/

/*
 * Pentium performance counter driver for OpenBSD.
 * Copyright 1996 David Mazieres <dm@lcs.mit.edu>.
 *
 * Modification and redistribution in source and binary forms is
 * permitted provided that due credit is given to the author and the
 * OpenBSD project by leaving this copyright notice intact.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/systm.h>

#include <machine/cputypes.h>
#include <machine/psl.h>
#include <machine/pctr.h>
#include <machine/cpu.h>
#include <machine/specialreg.h>

pctrval pctr_idlcnt;  /* Gets incremented in locore.s */

/* Pull in the cpuid values from locore.s */
extern int cpu_id;
extern int cpu_feature;
extern char cpu_vendor[];

int pctr_isintel;

#define usetsc		(cpu_feature & CPUID_TSC)
#define usep5ctr	(pctr_isintel && (((cpu_id >> 8) & 15) == 5) && \
				(((cpu_id >> 4) & 15) > 0))
#define usep6ctr	(pctr_isintel && ((cpu_id >> 8) & 15) == 6)

void pctrattach(int);
int pctropen(dev_t, int, int, struct proc *);
int pctrclose(dev_t, int, int, struct proc *);
int pctrioctl(dev_t, u_long, caddr_t, int, struct proc *);
int p5ctrsel(int fflag, u_int cmd, u_int fn);
static __inline void p5ctrrd(struct pctrst *st);
int p6ctrsel(int fflag, u_int cmd, u_int fn);
static __inline void p6ctrrd(struct pctrst *st);

void
pctrattach (num)
	int num;
{
	if (num > 1)
		return;

	pctr_isintel = (strcmp(cpu_vendor, "GenuineIntel") == 0);

	if (usep6ctr)
		/* Enable RDTSC and RDPMC instructions from user-level. */
		__asm __volatile ("movl %%cr4,%%eax\n"
				  "\tandl %0,%%eax\n"
				  "\torl %1,%%eax\n"
				  "\tmovl %%eax,%%cr4"
				  :: "i" (~CR4_TSD), "i" (CR4_PCE) : "eax");
	else if (usetsc)
		/* Enable RDTSC instruction from user-level. */
		__asm __volatile ("movl %%cr4,%%eax\n"
				  "\tandl %0,%%eax\n"
				  "\tmovl %%eax,%%cr4"
				  :: "i" (~CR4_TSD) : "eax");

	if (usep6ctr)
		printf ("pctr: 686-class user-level performance counters enabled\n");
	else if (usep5ctr)
		printf ("pctr: 586-class performance counters and user-level cycle counter enabled\n");
	else if (usetsc)
		printf ("pctr: user-level cycle counter enabled\n");
	else
		printf ("pctr: no performance counters in CPU\n");
}

int
pctropen (dev, oflags, devtype, p)
	dev_t dev;
	int oflags;
	int devtype;
	struct proc *p;
{
	if (minor (dev))
		return ENXIO;
	return 0;
}

int
pctrclose (dev, oflags, devtype, p)
	dev_t dev;
	int oflags;
	int devtype;
	struct proc *p;
{
	return 0;
}

int
p5ctrsel (fflag, cmd, fn)
	int fflag;
	u_int cmd;
	u_int fn;
{
	pctrval msr11;
	int msr;
	int shift;

	cmd -= PCIOCS0;
	if (cmd > 1)
		return EINVAL;
	msr = P5MSR_CTR0 + cmd;
	shift = cmd ? 0x10 : 0;

	if (! (fflag & FWRITE))
		return EPERM;
	if (fn >= 0x200)
		return EINVAL;

	msr11 = rdmsr (P5MSR_CTRSEL);
	msr11 &= ~(0x1ffLL << shift);
	msr11 |= fn << shift;
	wrmsr (P5MSR_CTRSEL, msr11);
	wrmsr (msr, 0);

	return 0;
}

static __inline void
p5ctrrd (st)
	struct pctrst *st;
{
	u_int msr11;

	msr11 = rdmsr (P5MSR_CTRSEL);
	st->pctr_fn[0] = msr11 & 0xffff;
	st->pctr_fn[1] = msr11 >> 16;
	__asm __volatile ("cli");
	st->pctr_tsc = rdtsc ();
	st->pctr_hwc[0] = rdmsr (P5MSR_CTR0);
	st->pctr_hwc[1] = rdmsr (P5MSR_CTR1);
	__asm __volatile ("sti");
}

int
p6ctrsel (fflag, cmd, fn)
	int fflag;
	u_int cmd;
	u_int fn;
{
	int msrsel, msrval;

	cmd -= PCIOCS0;
	if (cmd > 1)
		return EINVAL;
	msrsel = P6MSR_CTRSEL0 + cmd;
	msrval = P6MSR_CTR0 + cmd;

	if (! (fflag & FWRITE))
		return EPERM;
	if (fn & 0x380000)
		return EINVAL;

	wrmsr (msrval, 0);
	wrmsr (msrsel, fn);
	wrmsr (msrval, 0);

	return 0;
}

static __inline void
p6ctrrd (st)
	struct pctrst *st;
{
	st->pctr_fn[0] = rdmsr (P6MSR_CTRSEL0);
	st->pctr_fn[1] = rdmsr (P6MSR_CTRSEL1);
	__asm __volatile ("cli");
	st->pctr_tsc = rdtsc ();
	st->pctr_hwc[0] = rdpmc (0);
	st->pctr_hwc[1] = rdpmc (1);
	__asm __volatile ("sti");
}


int
pctrioctl (dev, cmd, data, fflag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int fflag;
	struct proc *p;
{
	switch (cmd) {
	case PCIOCRD:
	{
		struct pctrst *st = (void *) data;
		
		if (usep6ctr)
			p6ctrrd (st);
		else if (usep5ctr)
			p5ctrrd (st);
		else {
			bzero (st, sizeof (*st));
			if (usetsc)
				st->pctr_tsc = rdtsc ();
		}
		st->pctr_idl = pctr_idlcnt;
		return 0;
	}
	case PCIOCS0:
	case PCIOCS1:
		if (usep6ctr)
			return p6ctrsel (fflag, cmd, *(u_int *) data);
		if (usep5ctr)
			return p5ctrsel (fflag, cmd, *(u_int *) data);
		return ENODEV;
	default:
		return EINVAL;
	}
}
