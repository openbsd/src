/*	$OpenBSD: bug.h,v 1.8 2001/11/27 05:36:40 miod Exp $ */

#ifndef _MACHINE_BUG_H_
#define _MACHINE_BUG_H_

struct bugenv {
	int	clun;
	int	dlun;
	int	ipl;
	int	ctlr;
	int	(*entry) __P((void));
	int	cfgblk;
	char	*argstart;
	char	*argend;
};

#endif	/* _MACHINE_BUG_H_ */
