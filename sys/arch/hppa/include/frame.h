/*	$OpenBSD: frame.h,v 1.2 1998/07/07 21:32:40 mickey Exp $	*/


#ifndef _HPPA_FRAME_H_
#define _HPPA_FRAME_H_

#define	FRAME_PC	0

#ifndef _LOCORE
struct trapframe {
	u_int	flags;
	u_int	r1;
	u_int	rp;          /* r2 */
	u_int	r3;          /* frame pointer when -g */
	u_int	r4;
	u_int	r5;
	u_int	r6;
	u_int	r7;
	u_int	r8;
	u_int	r9;
	u_int	r10;
	u_int	r11;
	u_int	r12;
	u_int	r13;
	u_int	r14;
	u_int	r15;
	u_int	r16;
	u_int	r17;
	u_int	r18;
	u_int	t4;          /* r19 */
	u_int	t3;          /* r20 */
	u_int	t2;          /* r21 */
	u_int	t1;          /* r22 */
	u_int	arg3;        /* r23 */
	u_int	arg2;        /* r24 */
	u_int	arg1;	      /* r25 */
	u_int	arg0;	      /* r26 */
	u_int	dp;	      /* r27 */
	u_int	ret0;	      /* r28 */
	u_int	ret1;	      /* r29 */
	u_int	sp;	      /* r30 */
	u_int	r31;
	u_int	sar;	      /* cr11 */
	u_int	iioq_head;
	u_int	iisq_head;
	u_int	iioq_tail;
	u_int	iisq_tail;
	u_int	eiem;        /* cr15 */
	u_int	iir;         /* cr19 */
	u_int	isr;         /* cr20 */
	u_int	ior;         /* cr21 */
	u_int	ipsw;	      /* cr22 */
	u_int	sr4;
	u_int	sr0;
	u_int	sr1;
	u_int	sr2;
	u_int	sr3;
	u_int	sr5;
	u_int	sr6;
	u_int	sr7;
	u_int	rctr;         /* cr0 */
	u_int	pidr1;        /* cr8 */
	u_int	pidr2;        /* cr9 */
	u_int	ccr;	       /* cr10 */
	u_int	pidr3;        /* cr12 */
	u_int	pidr4;        /* cr13 */
	u_int	ptov;         /* cr24 */
	u_int	tr1;          /* cr25 */
	u_int	tr2;          /* cr26 */
	u_int	fpu; 

	int tf_regs[10];
};
#endif /* !_LOCORE */

#endif /* !_HPPA_FRAME_H_ */
