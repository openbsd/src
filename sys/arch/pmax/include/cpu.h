/*	$NetBSD: cpu.h,v 1.15 1996/05/19 01:28:47 jonathan Exp $	*/

#include <mips/cpu.h>
#include <mips/cpuregs.h> /* XXX */

#define	CLKF_USERMODE(framep)	CLKF_USERMODE_R3K(framep)
#define	CLKF_BASEPRI(framep)	CLKF_BASEPRI_R3K(framep)


#ifdef _KERNEL
union	cpuprid cpu_id;
union	cpuprid fpu_id;
u_int	machDataCacheSize;
u_int	machInstCacheSize;
extern	struct intr_tab intr_tab[];
#endif
