/*	$OpenBSD: bug.h,v 1.7 2001/08/26 14:31:07 miod Exp $ */
#ifndef __MACHINE_BUG_H__
#define __MACHINE_BUG_H__
#include <machine/bugio.h>

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
#endif /* __MACHINE_BUG_H__ */
