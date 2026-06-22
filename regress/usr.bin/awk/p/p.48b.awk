BEGIN	{ srand(0); k = 3; n = 10 }
{	if (n <= 0) exit
	if (rand() <= k/n) { print; k-- }
	n--
}
