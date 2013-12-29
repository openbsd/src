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
