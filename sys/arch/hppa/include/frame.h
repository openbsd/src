/*	$OpenBSD: frame.h,v 1.9 1999/09/11 23:49:37 mickey Exp $	*/

/*
 * Copyright (c) 1999 Michael Shalayeff
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
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef _MACHINE_FRAME_H_
#define _MACHINE_FRAME_H_

#include <machine/reg.h>

/*
 * Call frame definitions
 */
#define	HPPA_FRAME_NARGS	(12)
#define	HPPA_FRAME_MAXARGS	(HPPA_FRAME_NARGS * 4)
#define	HPPA_FRAME_ARG(n)	(-(32 + 4*(n)))
#define	HPPA_FRAME_CARG(n,sp)	((register_t *)((sp) + HPPA_FRAME_ARG(n)))
#define	HPPA_FRAME_SIZE		(48)
#define	HPPA_FRAME_PSP		(-4)
#define	HPPA_FRAME_EP		(-8)
#define	HPPA_FRAME_CLUP		(-12)
#define	HPPA_FRAME_SL		(-16)
#define	HPPA_FRAME_CRP		(-20)
#define	HPPA_FRAME_ERP		(-24)
#define	HPPA_FRAME_ESR4		(-28)
#define	HPPA_FRAME_EDP		(-32)

/*
 * Macros to decode processor status word.
 */
#define	HPPA_PC_PRIV_MASK    3
#define	HPPA_PC_PRIV_KERN    0
#define	HPPA_PC_PRIV_USER    3
#define	USERMODE(pc)    ((((register_t)pc) & HPPA_PC_PRIV_MASK) != HPPA_PC_PRIV_KERN)
#define	KERNMODE(pc)	(((register_t)pc) & ~HPPA_PC_PRIV_MASK)

#ifndef _LOCORE
struct trapframe {
	u_int	tf_flags;
	u_int	tf_r1;
	u_int	tf_rp;          /* r2 */
	u_int	tf_r3;          /* frame pointer when -g */
	u_int	tf_r4;
	u_int	tf_r5;
	u_int	tf_r6;
	u_int	tf_r7;
	u_int	tf_r8;
	u_int	tf_r9;
	u_int	tf_r10;
	u_int	tf_r11;
	u_int	tf_r12;
	u_int	tf_r13;
	u_int	tf_r14;
	u_int	tf_r15;
	u_int	tf_r16;
	u_int	tf_r17;
	u_int	tf_r18;
	u_int	tf_t4;		/* r19 */
	u_int	tf_t3;		/* r20 */
	u_int	tf_t2;		/* r21 */
	u_int	tf_t1;		/* r22 */
	u_int	tf_arg3;	/* r23 */
	u_int	tf_arg2;	/* r24 */
	u_int	tf_arg1;	/* r25 */
	u_int	tf_arg0;	/* r26 */
	u_int	tf_dp;		/* r27 */
	u_int	tf_ret0;	/* r28 */
	u_int	tf_ret1;	/* r29 */
	u_int	tf_sp;		/* r30 */
	u_int	tf_r31;
	u_int	tf_sr0;
	u_int	tf_sr1;
	u_int	tf_sr2;
	u_int	tf_sr3;
	u_int	tf_sr4;
	u_int	tf_sr5;
	u_int	tf_sr6;
	u_int	tf_sr7;
	u_int	tf_pidr1;	/* cr8 */
	u_int	tf_pidr2;	/* cr9 */
	u_int	tf_pidr3;	/* cr12 */
	u_int	tf_pidr4;	/* cr13 */
	u_int	tf_rctr;	/* cr0 */
	u_int	tf_eiem;	/* cr15 */
	u_int	tf_ccr;		/* cr10 */
	u_int	tf_sar;		/* cr11 */
	u_int	tf_iisq_head;	/* cr17 */
	u_int	tf_iisq_tail;
	u_int	tf_iioq_head;	/* cr18 */
	u_int	tf_iioq_tail;
	u_int	tf_iir;		/* cr19 */
	u_int	tf_isr;		/* cr20 */
	u_int	tf_ior;		/* cr21 */
	u_int	tf_ipsw;	/* cr22 */
	u_int	tf_eirr;	/* cr23 - DDB */
	u_int	tf_hptm;	/* cr24 - DDB */
	u_int	tf_vtop;	/* cr25 - DDB */
	u_int	tf_cr28;	/*      - DDB */
	u_int	tf_cr30;	/* KSP */

	u_int	tf_pad[3];	/* pad to 256 bytes */
};
#endif /* !_LOCORE */

#endif /* !_MACHINE_FRAME_H_ */
