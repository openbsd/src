/*	$OpenBSD: bug.h,v 1.4 1999/02/09 06:36:25 smurph Exp $ */
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
