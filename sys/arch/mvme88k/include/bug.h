/*	$OpenBSD: bug.h,v 1.6 2001/08/12 12:03:02 heko Exp $ */
#ifndef __MACHINE_BUG_H__
#define __MACHINE_BUG_H__
#include <machine/bugio.h>

struct bugenv {
	int	clun;
	int	dlun;
	int	ipl;
	int	ctlr;
	int	(*entry)();
	int	cfgblk;
	char	*argstart;
	char	*argend;
};
#endif /* __MACHINE_BUG_H__ */
