/*	$OpenBSD: memdevs.h,v 1.2 2002/03/14 01:26:39 millert Exp $	*/

#ifndef _MVME88K_MEMDEVS_H_
#define _MVME88K_MEMDEVS_H_

int memdevrw(caddr_t base, int len, struct uio *uio, int flags);

#endif	/* _MVME88K_MEMDEVS_H_ */
