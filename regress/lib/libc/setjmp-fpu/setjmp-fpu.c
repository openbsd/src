#include <err.h>
#include <fenv.h>
#include <setjmp.h>

int
TEST_SETJMP(void)
{
	JMP_BUF env;
	int rv;

	/* Set up the FPU control word register. */
	rv = fesetround(FE_UPWARD);
	if (rv != 0)
		errx(1, "fesetround FE_UPWARD returned %d", rv);
	fedisableexcept(FE_ALL_EXCEPT);
	feenableexcept(FE_DIVBYZERO);

	rv = SETJMP(env, 0);

	switch(rv) {
	case 0: {
		fexcept_t flag = FE_OVERFLOW;

		/* Mess with the FPU control word. */
		rv = fesetround(FE_DOWNWARD);
		if (rv != 0)
			errx(1, "fesetround FE_DOWNWARD returned %d", rv);
		fedisableexcept(FE_DIVBYZERO);

		/* Set the FPU exception flags. */
		rv = fesetexceptflag(&flag, FE_ALL_EXCEPT);
		if (rv != 0)
			errx(1, "fesetexceptflag returned %d", rv);

		LONGJMP(env, 1);
		errx(1, "longjmp returned");
	}
	case 1: {
		fexcept_t flag = 0;

		/* Verify that the FPU control word is preserved. */
		rv = fegetround();
		if (rv != FE_UPWARD)
			errx(1, "fegetround returned %d, not FE_UPWARD", rv);
		rv = fegetexcept();
		if (rv != FE_DIVBYZERO)
			errx(1, "fegetexcept returned %d, not FE_DIVBYZERO",
			    rv);

		/* Verify that the FPU exception flags weren't clobbered. */
		rv = fegetexceptflag(&flag, FE_ALL_EXCEPT);
		if (rv != 0)
			errx(1, "fegetexceptflag returned %d", rv);
		if (flag != FE_OVERFLOW)
			errx(1, "except flag is %d, no FE_OVERFLOW", rv);

		return (0);
	}
	default:
		errx(1, "setjmp returned %d", rv);
	}
}
