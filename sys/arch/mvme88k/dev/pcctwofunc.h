/*	$OpenBSD: pcctwofunc.h,v 1.4 2002/03/14 01:26:39 millert Exp $ */

#ifndef _MVME88K_PCCTWO_H_
#define _MVME88K_PCCTWO_H_

int pcctwointr_establish(int vec, struct intrhand *ih);

#endif	/* _MVME88K_PCCTWO_H_ */

