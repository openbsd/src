/*	$OpenBSD: ieeefp.h,v 1.3 2002/02/16 21:27:17 millert Exp $	*/

/* 
 * Written by J.T. Conklin, Apr 6, 1995
 * Public domain.
 */

#ifndef _IEEEFP_H_
#define _IEEEFP_H_

#include <sys/cdefs.h>
#include <machine/ieeefp.h>

extern fp_rnd    fpgetround(void);
extern fp_rnd    fpsetround(fp_rnd);
extern fp_except fpgetmask(void);
extern fp_except fpsetmask(fp_except);
extern fp_except fpgetsticky(void);
extern fp_except fpsetsticky(fp_except);

#endif /* _IEEEFP_H_ */
