struct bugenv {
	int	clun;
	int	dlun;
	int	ipl;
	int	(*entry)();
	char	bootargs[256];
};

