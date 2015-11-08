#include <fenv.h>
#include <setjmp.h>

int
TEST_SETJMP(int argc, char *argv[])
{
	JMP_BUF env;
	int rv;

	/* Set up the FPU control word register. */
	fesetround(FE_UPWARD);
	fedisableexcept(FE_ALL_EXCEPT);
	feenableexcept(FE_DIVBYZERO);

	rv = SETJMP(env, 0);

	if (rv == 0) {
		fexcept_t flag = FE_OVERFLOW;

		/* Mess with the FPU control word. */
		fesetround(FE_DOWNWARD);
		fedisableexcept(FE_DIVBYZERO);

		/* Set the FPU exception flags. */
		fesetexceptflag(&flag, FE_ALL_EXCEPT);

		LONGJMP(env, 1);
	} else if (rv == 1) {
		fexcept_t flag = 0;

		/* Verify that the FPU control word is preserved. */
		if (fegetround() != FE_UPWARD
		    || fegetexcept() != FE_DIVBYZERO)
			return (1);

		/* Verify that the FPU exception flags weren't clobbered. */
		fegetexceptflag(&flag, FE_ALL_EXCEPT);
		if (flag != FE_OVERFLOW)
			return (1);

		return (0);
	}

	/* This is not supposed to happen. */
	return (1);
}
