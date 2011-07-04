/*	$OpenBSD: ieeefp.h,v 1.1 2011/07/04 23:29:08 pirofti Exp $	*/

/* 
 * Written by J.T. Conklin, Apr 6, 1995
 * Public domain.
 */

#ifndef _IA64_IEEEFP_H_
#define _IA64_IEEEFP_H_

typedef int fp_except;
#define FP_X_INV	0x01	/* invalid operation exception */
#define FP_X_DNML	0x02	/* denormalization exception */
#define FP_X_DZ		0x04	/* divide-by-zero exception */
#define FP_X_OFL	0x08	/* overflow exception */
#define FP_X_UFL	0x10	/* underflow exception */
#define FP_X_IMP	0x20	/* imprecise (loss of precision) */

typedef enum {
    FP_RN=0,			/* round to nearest representable number */
    FP_RM=1,			/* round toward negative infinity */
    FP_RP=2,			/* round toward positive infinity */
    FP_RZ=3			/* round to zero (truncate) */
} fp_rnd;

#endif /* _IA64_IEEEFP_H_ */
