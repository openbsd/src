/* 
 * Written by J.T. Conklin, Apr 28, 1995
 * Public domain.
 */

#ifndef _PC532_IEEEFP_H_
#define _PC532_IEEEFP_H_

/* defined just to keep prototypes in machine independant header happy. */
typedef int fp_except;

typedef enum {
    FP_RN=0,			/* round to nearest representable number */
    FP_RZ=1,			/* round to zero (truncate) */
    FP_RP=2,			/* round toward positive infinity */
    FP_RM=3			/* round toward negative infinity */
} fp_rnd;

#endif /* _PC532_IEEEFP_H_ */
