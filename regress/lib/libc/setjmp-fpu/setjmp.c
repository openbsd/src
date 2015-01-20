/*	$OpenBSD: setjmp.c,v 1.2 2015/01/20 04:41:01 krw Exp $	*/
#define	SETJMP(env, savemask)	setjmp(env)
#define	LONGJMP(env, val)	longjmp(env, val)
#define	TEST_SETJMP		test_setjmp

#include "setjmp-fpu.c"
