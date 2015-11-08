/*	$OpenBSD: _setjmp.c,v 1.3 2015/11/08 18:10:14 miod Exp $	*/
#define	SETJMP(env, savemask)	_setjmp(env)
#define	LONGJMP(env, val)	_longjmp(env, val)
#define	TEST_SETJMP		test__setjmp
#define	JMP_BUF			jmp_buf

#include "setjmp-fpu.c"
