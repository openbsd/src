/*	$OpenBSD: memdevs.h,v 1.1 2001/03/08 22:21:49 miod Exp $	*/

#ifndef _MVME88K_MEMDEVS_H_
#define _MVME88K_MEMDEVS_H_

int memdevrw __P((caddr_t base, int len, struct uio *uio, int flags));

#endif	/* _MVME88K_MEMDEVS_H_ */
