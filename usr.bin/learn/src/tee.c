char *PS1;

main()
{
	int f;
	char c;
	char *getenv(char *);

	PS1 = getenv("PS1");
	if (PS1==0)
		PS1 = "$ ";
	f = creat(".ocopy", 0666);
	while (read(0, &c, 1) == 1) {
		write (1, &c, 1);
		put(c, f);
	}
	fl(f);
	close(f);
}

static char ln[5120];
char *p = ln;
put(c, f)
{
	*p++ = c;
	if (c == '\n') {
		fl(f);
		p=ln;
	}
}
fl(f)
{
	register char *s;

	s = ln;
	while (*s == '$' && *(s+1) == ' ')
		s += 2;
	if (strncmp(s, PS1, strlen(PS1)) == 0)
		s += strlen(PS1);
	write(f, s, p-s);
}
