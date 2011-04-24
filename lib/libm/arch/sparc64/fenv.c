/*	$OpenBSD: fenv.c,v 1.1 2011/04/24 00:35:22 martynas Exp $	*/
/*	$NetBSD: fenv.c,v 1.1 2011/01/31 00:19:33 christos Exp $	*/

/*-
 * Copyright (c) 2004-2005 David Schultz <das@FreeBSD.ORG>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 */
#include <sys/cdefs.h>

#include <assert.h>
#include <fenv.h>

#define _DIAGASSERT(x) ((void) 0)

/* Load floating-point state register (all 64bits) */
#define	__ldxfsr(__r)	__asm__	__volatile__		\
	("ldx %0, %%fsr" : : "m" (__r))

/* Save floating-point state register (all 64bits) */
#define	__stxfsr(__r)	__asm__	__volatile__		\
	("stx %%fsr, %0" : "=m" (*(__r)))

/*
 * The feclearexcept() function clears the supported floating-point exceptions
 * represented by `excepts'.
 */
int
feclearexcept(int excepts)
{
	fexcept_t r;
	int ex;

	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	ex = excepts & FE_ALL_EXCEPT;

	__stxfsr(&r);
	r &= ~ex;
	__ldxfsr(r);

	/* Success */
	return 0;
}

/*
 * The fegetexceptflag() function stores an implementation-defined
 * representation of the states of the floating-point status flags indicated
 * by the argument excepts in the object pointed to by the argument flagp.
 */
int
fegetexceptflag(fexcept_t *flagp, int excepts)
{
	fexcept_t r;
	int ex;

	_DIAGASSERT(flagp != NULL);
	_DIAGASSERT((excepts & ~_FE_ALL_EXCEPT) == 0);

	ex = excepts & FE_ALL_EXCEPT;

	__stxfsr(&r);
	*flagp = r & ex;

	/* Success */
	return 0;
}


/*
 * This function sets the floating-point status flags indicated by the argument
 * `excepts' to the states stored in the object pointed to by `flagp'. It does
 * NOT raise any floating-point exceptions, but only sets the state of the flags.
 */
int
fesetexceptflag(const fexcept_t *flagp, int excepts)
{
	fexcept_t r;
	int ex;

	_DIAGASSERT(flagp != NULL);
	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	ex = excepts & FE_ALL_EXCEPT;

	__stxfsr(&r);
	r &= ~ex;
	r |= *flagp & ex;
	__ldxfsr(r);

	/* Success */
	return 0;
}

/*
 * The feraiseexcept() function raises the supported floating-point exceptions
 * represented by the argument `excepts'.
 *
 * The order in which these floating-point exceptions are raised is unspecified
 * (by the standard).
 */
int
feraiseexcept(int excepts)
{
	volatile double d;
	int ex;

	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	ex = excepts & FE_ALL_EXCEPT;

	/*
	 * With a compiler that supports the FENV_ACCESS pragma properly, simple
	 * expressions like '0.0 / 0.0' should be sufficient to generate traps.
	 * Unfortunately, we need to bring a volatile variable into the equation
	 * to prevent incorrect optimizations.
	 */
	if (ex & FE_INVALID) {
		d = 0.0;
		d = 0.0 / d;
	}
	if (ex & FE_DIVBYZERO) {
		d = 0.0;
		d = 1.0 / d;
	}
	if (ex & FE_OVERFLOW) {
		d = 0x1.ffp1023;
		d *= 2.0;
	}
	if (ex & FE_UNDERFLOW) {
		d = 0x1p-1022;
		d /= 0x1p1023;
	}
	if (ex & FE_INEXACT) {
		d = 0x1p-1022;
		d += 1.0;
	}

	/* Success */
	return 0;
}

/*
 * The fetestexcept() function determines which of a specified subset of the
 * floating-point exception flags are currently set. The `excepts' argument
 * specifies the floating-point status flags to be queried.
 */
int
fetestexcept(int excepts)
{
	fexcept_t r;

	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	__stxfsr(&r);

	return r & (excepts & FE_ALL_EXCEPT);
}

/*
 * The fegetround() function gets the current rounding direction.
 */
int
fegetround(void)
{
	fenv_t r;

	__stxfsr(&r);

	return (r >> _ROUND_SHIFT) & _ROUND_MASK;
}

/*
 * The fesetround() function establishes the rounding direction represented by
 * its argument `round'. If the argument is not equal to the value of a rounding
 * direction macro, the rounding direction is not changed.
 */
int
fesetround(int round)
{
	fenv_t r;

	_DIAGASSERT((round & ~_ROUND_MASK) == 0);
	if (round & ~_ROUND_MASK)
		return -1;

	__stxfsr(&r);
	r &= ~(_ROUND_MASK << _ROUND_SHIFT);
	r |= round << _ROUND_SHIFT;
	__ldxfsr(r);

	/* Success */
	return 0;
}

/*
 * The fegetenv() function attempts to store the current floating-point
 * environment in the object pointed to by envp.
 */
int
fegetenv(fenv_t *envp)
{
	_DIAGASSERT(envp != NULL);

	__stxfsr(envp);

	/* Success */
	return 0;
}


/*
 * The feholdexcept() function saves the current floating-point environment
 * in the object pointed to by envp, clears the floating-point status flags, and
 * then installs a non-stop (continue on floating-point exceptions) mode, if
 * available, for all floating-point exceptions.
 */
int
feholdexcept(fenv_t *envp)
{
	fenv_t r;

	_DIAGASSERT(envp != NULL);

	__stxfsr(&r);
	*envp = r;
	r &= ~(FE_ALL_EXCEPT | _ENABLE_MASK);
	__ldxfsr(r);

	/* Success */
	return 0;
}

/*
 * The fesetenv() function attempts to establish the floating-point environment
 * represented by the object pointed to by envp. The argument `envp' points
 * to an object set by a call to fegetenv() or feholdexcept(), or equal a
 * floating-point environment macro. The fesetenv() function does not raise
 * floating-point exceptions, but only installs the state of the floating-point
 * status flags represented through its argument.
 */
int
fesetenv(const fenv_t *envp)
{
	_DIAGASSERT(envp != NULL);

	__ldxfsr(*envp);

	/* Success */
	return 0;
}


/*
 * The feupdateenv() function saves the currently raised floating-point
 * exceptions in its automatic storage, installs the floating-point environment
 * represented by the object pointed to by `envp', and then raises the saved
 * floating-point exceptions. The argument `envp' shall point to an object set
 * by a call to feholdexcept() or fegetenv(), or equal a floating-point
 * environment macro.
 */
int
feupdateenv(const fenv_t *envp)
{
	fexcept_t r;

	_DIAGASSERT(envp != NULL);

	__stxfsr(&r);
	__ldxfsr(*envp);

	_DIAGASSERT((r & ~FE_ALL_EXCEPT) == 0);
	feraiseexcept(r & FE_ALL_EXCEPT);

	/* Success */
	return 0;
}

/*
 * The following functions are extentions to the standard
 */
int
feenableexcept(int mask)
{
	fenv_t old_r, new_r;

	__stxfsr(&old_r);
	new_r = old_r | ((mask & FE_ALL_EXCEPT) << _FPUSW_SHIFT);
	__ldxfsr(new_r);

	return (old_r >> _FPUSW_SHIFT) & FE_ALL_EXCEPT;
}

int
fedisableexcept(int mask)
{
	fenv_t old_r, new_r;

	__stxfsr(&old_r);
	new_r = old_r & ~((mask & FE_ALL_EXCEPT) << _FPUSW_SHIFT);
	__ldxfsr(new_r);

	return (old_r >> _FPUSW_SHIFT) & FE_ALL_EXCEPT;
}

int
fegetexcept(void)
{
	fenv_t r;

	__stxfsr(&r);
	return (r & _ENABLE_MASK) >> _FPUSW_SHIFT;
}
