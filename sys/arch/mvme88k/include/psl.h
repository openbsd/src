#ifndef __M88K_M88100_PSL_H__
#define __M88K_M88100_PSL_H__

/* needs major cleanup - XXX nivas */

#define spl0()		spln(0)
#define spl1()		spln(1)
#define spl2()		spln(2)
#define spl3()		spln(3)
#define spl4()		spln(4)
#define spl5()		spln(5)
#define spl6()		spln(6)
#define spl7()		spln(7)

#define splnone()	spln(0)
#define splsoftclock()	spln(1)
#define splnet()	spln(1)
#define splbio()	spln(3)
#define splimp()	spln(3)
#define spltty()	spln(4)
#define splclock()	spln(6)
#define splstatclock()	spln(6)
#define splvm()		spln(6)
#define splhigh()	spln(7)
#define splsched()	spln(7)

#define splx(x)		spln(x)

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
#define PSR_MODE	0x80000000U	/* supervisor/user mode */
#define PSR_BO		0x40000000U	/* byte-ordering 0:big 1:little */
#define PSR_SER		0x20000000U	/* serial mode */
#define PSR_C		0x10000000U	/* carry */
#define PSR_SFD		0x000003F0U	/* SFU disable */
#define PSR_SFD1	0x00000008U	/* SFU1 (FPU) disable */
#define PSR_MXM		0x00000004U	/* misaligned access enable */
#define PSR_IND		0x00000002U	/* interrupt disable */
#define PSR_SFRZ	0x00000001U	/* shadow freeze */

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
