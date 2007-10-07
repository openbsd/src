main()
{
	char c = 0x7f;
	short s = 0x7fff;
	int i = 0x7fffffff;
	long long ll = 0x7fffffffffffffffLL;

	if (c > i)
		exit(1);
	if (s > i)
		exit(1);
	if (i > ll)
		exit(1);

	if (c > 0x7fff)
		exit(1);
	if (s > 0x7fffffff)
		exit(1);
	if (i > 0x7fffffffffffffffLL)
		exit(1);
	exit(0);
}

