/*	$OpenBSD: busswfunc.h,v 1.3 2001/12/19 04:02:25 smurph Exp $ */

#ifndef _MVME88K_BUSSWF_H_
#define _MVME88K_BUSSWF_H_

int busswintr_establish __P((int vec, struct intrhand *ih));

#endif	/* _MVME88K_PCCTWO_H_ */

