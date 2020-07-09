/* Public domain. */

#ifndef _ASM_SMP_H
#define _ASM_SMP_H

#if defined(__i386__) || defined(__amd64__)

#include <machine/cpu.h>

static inline int
wbinvd_on_all_cpus(void)
{
	/* XXX single cpu only */
	wbinvd();
	return 0;
}

#endif

#endif
