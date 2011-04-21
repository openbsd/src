/*	$OpenBSD: frame.h,v 1.2 2011/04/21 13:13:16 jsing Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _MACHINE_FRAME_H_
#define _MACHINE_FRAME_H_

/*
 * Call frame definitions
 */
#define	HPPA_FRAME_SIZE		(128)
#define	HPPA_FRAME_PSP		(-8)
#define	HPPA_FRAME_RP		(-16)
#define	HPPA_FRAME_ARG(n)       (-(16 + 8 * ((n) + 1)))

/*
 * Macros to decode processor status word.
 */
#define	HPPA_PC_PRIV_MASK    3
#define	HPPA_PC_PRIV_KERN    0
#define	HPPA_PC_PRIV_USER    3
#define	USERMODE(pc)    ((((register_t)pc) & HPPA_PC_PRIV_MASK) != HPPA_PC_PRIV_KERN)
#define	KERNMODE(pc)	(((register_t)pc) & ~HPPA_PC_PRIV_MASK)

/*
 *
 */
#define	HPPA_SID_MAX	0x7ffffe00
#define	HPPA_SID_KERNEL 0
#define	HPPA_PID_KERNEL 2

#ifndef _LOCORE
/*
 * the trapframe is divided into two parts:
 *	one is saved while we are in the physical mode (beginning of the trap),
 *	and should be kept as small as possible, since all the interrupts will
 *	be lost during this phase, also it must be 64-bytes aligned, per
 *	pa-risc stack conventions, and its dependencies in the code (;
 *	the other part is filled out when we are already in the virtual mode,
 *	are able to catch interrupts (they are kept pending) and perform
 *	other trap activities (like tlb misses).
 */
struct trapframe {
	unsigned long	tf_flags;
	unsigned long	tf_r1;
	unsigned long	tf_rp;
	unsigned long	tf_r3;
	unsigned long	tf_r4;
	unsigned long	tf_r5;
	unsigned long	tf_r6;
	unsigned long	tf_r7;
	unsigned long	tf_r8;
	unsigned long	tf_r9;
	unsigned long	tf_r10;
	unsigned long	tf_r11;
	unsigned long	tf_r12;
	unsigned long	tf_r13;
	unsigned long	tf_r14;
	unsigned long	tf_r15;
	unsigned long	tf_r16;
	unsigned long	tf_r17;
	unsigned long	tf_r18;
	unsigned long	tf_args[8];
	unsigned long	tf_dp;		/* r27 */
	unsigned long	tf_ret0;
	unsigned long	tf_ret1;
	unsigned long	tf_sp;
	unsigned long	tf_r31;
	unsigned long	tf_sr0;
	unsigned long	tf_sr1;
	unsigned long	tf_sr2;
	unsigned long	tf_sr3;
	unsigned long	tf_sr4;
	unsigned long	tf_sr5;
	unsigned long	tf_sr6;
	unsigned long	tf_sr7;
	unsigned long	tf_rctr;
	unsigned long	tf_ccr;		/* cr10 */
	unsigned long	tf_iioq[2];
	unsigned long	tf_iisq[2];
	unsigned long	tf_pidr1;
	unsigned long	tf_pidr2;
	unsigned long	tf_eiem;
	unsigned long	tf_eirr;
	unsigned long	tf_ior;
	unsigned long	tf_isr;
	unsigned long	tf_iir;
	unsigned long	tf_ipsw;
	unsigned long	tf_ci;		/* cr24 */
	unsigned long	tf_vtop;	/* cr25 */
	unsigned long	tf_cr30;	/* pa(u) */
	unsigned long	tf_cr27;	/* user curthread */
	unsigned long	tf_sar;

	unsigned long	tf_pad[5];
};
#endif /* !_LOCORE */

#endif /* !_MACHINE_FRAME_H_ */
