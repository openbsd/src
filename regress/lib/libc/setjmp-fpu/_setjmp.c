#define	SETJMP(env, savemask)	_setjmp(env)
#define	LONGJMP(env, val)	_longjmp(env, val)
#define	TEST_SETJMP		test__setjmp

#include "setjmp-fpu.c"
