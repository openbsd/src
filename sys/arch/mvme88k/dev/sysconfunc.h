/*	$OpenBSD: sysconfunc.h,v 1.1 2001/03/08 00:03:14 miod Exp $	*/

#ifndef _MVME88K_SYSCON_H_
#define _MVME88K_SYSCON_H_

int sysconintr_establish __P((int vec, struct intrhand *ih));

#endif	/* _MVME88K_SYSCON_H_ */
