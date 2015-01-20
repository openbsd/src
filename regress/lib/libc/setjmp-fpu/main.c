/*	$OpenBSD: main.c,v 1.2 2015/01/20 04:41:01 krw Exp $	*/
int	test__setjmp(void);
int	test_setjmp(void);
int	test_sigsetjmp(void);

int
main(int argc, char *argv[])
{
	return (test__setjmp()
		| test_setjmp()
		| test_sigsetjmp());
}
