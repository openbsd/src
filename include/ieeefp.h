/* 
 * Written by J.T. Conklin, Apr 6, 1995
 * Public domain.
 */

#ifndef _IEEEFP_H_
#define _IEEEFP_H_

#include <sys/cdefs.h>
#include <machine/ieeefp.h>

extern fp_rnd    fpgetround __P((void));
extern fp_rnd    fpsetround __P((fp_rnd));
extern fp_except fpgetmask __P((void));
extern fp_except fpsetmask __P((fp_except));
extern fp_except fpgetsticky __P((void));
extern fp_except fpsetsticky __P((fp_except));

#endif /* _IEEEFP_H_ */
