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
