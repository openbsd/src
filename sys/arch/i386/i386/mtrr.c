/* $OpenBSD: mtrr.c,v 1.1 1999/11/20 11:11:28 matthieu Exp $ */

#include <sys/param.h>
#include <sys/memrange.h>

#include <machine/specialreg.h>

/* Pull in the cpuid values from locore.s */
extern int cpu_id;
extern int cpu_feature;
extern char cpu_vendor[];

extern struct mem_range_ops i686_mrops;
extern struct mem_range_ops k6_mrops;

void mtrrattach __P((int));

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


