/*	$OpenBSD: cpu.h,v 1.2 1998/07/07 21:32:38 mickey Exp $	*/

/* 
 * Copyright (c) 1988-1994, The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: cpu.h 1.19 94/12/16$
 */

#ifndef	_HPPA_CPU_H_
#define	_HPPA_CPU_H_

#include <machine/frame.h>

/*
 * Exported definitions unique to hp700/PA-RISC cpu support.
 */

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#undef	COPY_SIGCODE		/* copy sigcode above user stack in exec */

/*
 * Supported systems
 */
#define	HP_810		0x100
#define	HP_815		0x103
#define	HP_822		0x101
#define	HP_825		0x008
#define	HP_827		0x102
#define	HP_835		0x00a
#define	HP_840		0x004
#define	HP_842		0x104
#define	HP_845		0x00b
#define	HP_850		0x080
#define	HP_852		0x105
#define	HP_855		0x081
#define	HP_860		0x082
#define	HP_870		0x083

#define	HP_720		0x200	/* [S] Cobra */
#define	HP_750		0x201	/* [S] Coral */
#define	HP_730		0x202	/* [S] King Cobra */

#define	HP_710		0x300	/* [S] Bushmaster */
#define	HP_705		0x302	/* [S] Bushmaster */

#define	HP_715_33	0x311	/* [S] Scorpio */
#define	HP_715_50	0x310	/* [S] Scorpio */
#define	HP_715T_50	0x312	/* [S] Scorpio (upgrade from 4XXt) */
#define	HP_715S_50	0x314	/* [S] Scorpio (upgrade from 4XXs) */
#define	HP_715_75	0x316	/* [S] Scorpio */
#define	HP_725		0xFFF	/* [S] Scorpio -- is this the real ID? */
#define	HP_735		0x203	/* [S] Hardball */
#define HP_735_125	0x206	/* [S] Hardball */
#define	HP_755		0xFFD	/* [S] Coral II -- is this the real ID? */

#define	HP_712_60	0x600	/* [S] Gekko */
#define	HP_712_80	0x601	/* [S] Gekko */
#define	HP_712_100	0x602	/* [S] Gekko */
#define	HP_743i_1	0x603
#define	HP_743i_2	0x604
#define	HP_712_4	0x605	/* [S] Gekko */
#define	HP_7GL_1	0x606
#define	HP_7GL_2	0x607
#define	HP_7GL_3	0x608
#define	HP_743v_1	0x617
#define	HP_743v_2	0x618
#define	HP_743i_3	0x619

#define	CPU_M770_100	0x585   /* J-class J200 */
#define	CPU_M770_120	0x586   /* J-class J210 */
#define	CPU_M777_100	0x592   /* C-class C200 */
#define	CPU_M777_120	0x58E   /* C-class C210 */

#define	HPPA_IOSPACE	0xf0000000
#define	HPPA_IOBCAST	0xfffc0000
#define	HPPA_PDC_LOW	0xef000000
#define	HPPA_PDC_HIGH	0xf1000000
#define	HPPA_FPA	0xfff80000
#define	HPPA_FLEX_DATA	0xfff80001
#define	HPPA_DMA_ENABLE	0x00000001
#define	HPPA_FLEX_MASK	0xfffc0000
#define	HPPA_SPA_ENABLE	0x00000020
#define	HPPA_NMODSPBUS	64

#define	CLKF_BASEPRI(framep)	(0)	/* XXX */
#define	CLKF_PC(framep)		(0)	/* XXX */
#define	CLKF_INTR(framep)	(0)	/* XXX */
#define	CLKF_USERMODE(framep)	(0)	/* XXX */

#define	signotify(p)		(void)(p)
#define	need_resched()		{(void)1;}
#define	need_proftick(p)	{(void)(p);}

/*
 * Expected (and optimized for) cache line size (in bytes).
 */
#define CACHE_LINE_SIZE 32

#ifdef _KERNEL
void	hppa_init __P((void));
#endif

/*
 * Boot arguments stuff
 */

#define	BOOTARG_LEN	(NBPG)
#define	BOOTARG_OFF	(NBPG)

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_MAXID		2	/* number of valid machdep ids */

#define CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
}

#endif /* _HPPA_CPU_H_ */
