#define	SETJMP(env, savemask)	sigsetjmp(env, savemask)
#define	LONGJMP(env, val)	siglongjmp(env, val)
#define	TEST_SETJMP		test_sigsetjmp

#include "setjmp-fpu.c"
