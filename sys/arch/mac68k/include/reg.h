/*	$NetBSD: reg.h,v 1.10 1996/05/05 06:18:00 briggs Exp $	*/

#ifndef _REG_MACHINE_
#define	_REG_MACHINE_

#include <machine/frame.h>
#include <m68k/reg.h>

__BEGIN_DECLS
/* machdep.c */
void	regdump __P((struct frame *, int));
__END_DECLS

#endif /* _REG_MACHINE_ */
