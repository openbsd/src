/*	$OpenBSD: reg.h,v 1.4 1997/11/30 06:12:35 gene Exp $	*/
/*	$NetBSD: reg.h,v 1.10 1996/05/05 06:18:00 briggs Exp $	*/

#ifndef _MAC68K_REG_H_
#define	_MAC68K_REG_H_

#include <machine/frame.h>
#include <m68k/reg.h>

__BEGIN_DECLS
/* machdep.c */
void	regdump __P((struct frame *, int));
__END_DECLS

#endif	/* _MAC68K_REG_H_ */
