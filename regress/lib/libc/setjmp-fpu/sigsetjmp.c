/*	$OpenBSD: sigsetjmp.c,v 1.2 2015/01/20 04:41:01 krw Exp $	*/
#define	SETJMP(env, savemask)	sigsetjmp(env, savemask)
#define	LONGJMP(env, val)	siglongjmp(env, val)
#define	TEST_SETJMP		test_sigsetjmp

#include "setjmp-fpu.c"
