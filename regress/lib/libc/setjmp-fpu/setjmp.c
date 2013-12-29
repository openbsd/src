#define	SETJMP(env, savemask)	setjmp(env)
#define	LONGJMP(env, val)	longjmp(env, val)
#define	TEST_SETJMP		test_setjmp

#include "setjmp-fpu.c"
