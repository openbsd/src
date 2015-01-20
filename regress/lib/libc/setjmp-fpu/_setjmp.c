/*	$OpenBSD: _setjmp.c,v 1.2 2015/01/20 04:41:01 krw Exp $	*/
#define	SETJMP(env, savemask)	_setjmp(env)
#define	LONGJMP(env, val)	_longjmp(env, val)
#define	TEST_SETJMP		test__setjmp

#include "setjmp-fpu.c"
