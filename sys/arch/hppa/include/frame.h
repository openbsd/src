/*	$OpenBSD: frame.h,v 1.1 1998/06/23 19:45:22 mickey Exp $	*/


#ifndef _HPPA_FRAME_H_
#define _HPPA_FRAME_H_

#define	FRAME_PC	0

struct trapframe {
	int i;
	int tf_regs[10];
};

#endif
