#include <fenv.h>
#include <setjmp.h>

int
main(int argc, char *argv[])
{
	jmp_buf env;
	int rv;

	/* Set up the FPU control word register. */
	fesetround(FE_UPWARD);
	fedisableexcept(FE_ALL_EXCEPT);
	feenableexcept(FE_DIVBYZERO);

	rv = setjmp(env);

	/* Mess with the FPU control word. */
	if (rv == 0) {
		fesetround(FE_DOWNWARD);
		fedisableexcept(FE_DIVBYZERO);
		longjmp(env, 1);
	/* Verify that the FPU control word is preserved. */
	} else if (rv == 1) {
		if (fegetround() != FE_UPWARD
		    || fegetexcept() != FE_DIVBYZERO)
			return (1);
		return (0);
	/* This is not supposed to happen. */
	} else {
		return (1);
	}

	return (1);
}
