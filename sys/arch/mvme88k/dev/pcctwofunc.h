/*	$OpenBSD: pcctwofunc.h,v 1.3 2001/12/16 23:49:46 miod Exp $ */

#ifndef _MVME88K_PCCTWO_H_
#define _MVME88K_PCCTWO_H_

int pcctwointr_establish __P((int vec, struct intrhand *ih));

#endif	/* _MVME88K_PCCTWO_H_ */

