/*	$OpenBSD: psl.h,v 1.4 1999/02/09 06:36:27 smurph Exp $ */
/*
 * Copyright (c) 1996 Nivas Madhur
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
 *      This product includes software developed by Nivas Madhur.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 */
#ifndef __M88K_M88100_PSL_H__
#define __M88K_M88100_PSL_H__

/* needs major cleanup - XXX nivas */

#if 0
spl0 is a function by itself. I really am serious about the clean up
above...
#define spl0()		spln(0)
#endif /* 0 */
#define spl1()		setipl(1)
#define spl2()		setipl(2)
#define spl3()		setipl(3)
#define spl4()		setipl(4)
#define spl5()		setipl(5)
#define spl6()		setipl(6)
#define spl7()		setipl(7)

/*
 * IPL levels.
 * We use 6 as IPL_HIGH so that abort can be programmed at 7 so that
 * it is always possible to break into the system unless interrupts
 * are disabled.
 */

#define IPL_NONE	0
#define IPL_SOFTCLOCK	1
#define IPL_SOFTNET	1
#define IPL_BIO		2
#define IPL_NET		3
#define IPL_TTY		3
#define IPL_CLOCK	5
#define IPL_STATCLOCK	5
#define IPL_IMP		6
#define IPL_VM		6
#define IPL_HIGH	6
#define IPL_SCHED	6
#define IPL_NMI		7
#define IPL_ABORT	7

#define splnone		spl0
#define splsoftclock()	setipl(IPL_SOFTCLOCK)
#define splsoftnet()	setipl(IPL_SOFTNET)
#define splbio()	setipl(IPL_BIO)
#define splnet()	setipl(IPL_NET)
#define spltty()	setipl(IPL_TTY)
#define splclock()	setipl(IPL_CLOCK)
#define splstatclock()	setipl(IPL_STATCLOCK)
#define splimp()	setipl(IPL_IMP)
#define splvm()		setipl(IPL_VM)
#define splhigh()	setipl(IPL_HIGH)
#define splsched()	setipl(IPL_SCHED)

#define splx(x)		((x) ? setipl((x)) : spl0())

/* 
 * 88100 control registers
 */

/*
 * processor identification register (PID)
 */
#define PID_ARN		0x0000FF00U	/* architectural revision number */
#define PID_VN		0x000000FEU	/* version number */
#define PID_MC		0x00000001U	/* master/checker */

/*
 * processor status register
 */
#ifndef PSR_MODE
#define PSR_MODE	0x80000000U	/* supervisor/user mode */
#endif
#define PSR_BO		0x40000000U	/* byte-ordering 0:big 1:little */
#define PSR_SER		0x20000000U	/* serial mode */
#define PSR_C		0x10000000U	/* carry */
#define PSR_SFD		0x000003F0U	/* SFU disable */
#define PSR_SFD1	0x00000008U	/* SFU1 (FPU) disable */
#ifndef PSR_MXM
#define PSR_MXM		0x00000004U	/* misaligned access enable */
#endif
#ifndef PSR_IND
#define PSR_IND		0x00000002U	/* interrupt disable */
#endif
#ifndef PSR_SFRZ
#define PSR_SFRZ	0x00000001U	/* shadow freeze */
#endif
/*
 *	This is used in ext_int() and hard_clock().
 */
#define PSR_IPL		0x00001000	/* for basepri */
#define PSR_IPL_LOG	12		/* = log2(PSR_IPL) */

#define PSR_MODE_LOG	31		/* = log2(PSR_MODE) */
#define PSR_BO_LOG	30		/* = log2(PSR_BO) */
#define PSR_SER_LOG	29		/* = log2(PSR_SER) */
#define PSR_SFD1_LOG	3		/* = log2(PSR_SFD1) */
#define PSR_MXM_LOG	2		/* = log2(PSR_MXM) */
#define PSR_IND_LOG	1		/* = log2(PSR_IND) */
#define PSR_SFRZ_LOG	0		/* = log2(PSR_SFRZ) */

#define PSR_SUPERVISOR	(PSR_MODE | PSR_SFD)
#define PSR_USER	(PSR_SFD)
#define PSR_SET_BY_USER	(PSR_BO | PSR_SER | PSR_C | PSR_MXM)

#ifndef	ASSEMBLER
struct psr {
    unsigned
	psr_mode: 1,
	psr_bo  : 1,
	psr_ser : 1,
	psr_c   : 1,
	        :18,
	psr_sfd : 6,
	psr_sfd1: 1,
	psr_mxm : 1,
	psr_ind : 1,
	psr_sfrz: 1;
};
#endif 

#define FIP_V		0x00000002U	/* valid */
#define FIP_E		0x00000001U	/* exception */
#define FIP_ADDR	0xFFFFFFFCU	/* address mask */
#define NIP_V		0x00000002U	/* valid */
#define NIP_E		0x00000001U	/* exception */
#define NIP_ADDR	0xFFFFFFFCU	/* address mask */
#define XIP_V		0x00000002U	/* valid */
#define XIP_E		0x00000001U	/* exception */
#define XIP_ADDR	0xFFFFFFFCU	/* address mask */

#endif /* __M88K_M88100_PSL_H__ */

