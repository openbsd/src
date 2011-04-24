/*	$OpenBSD: fenv.c,v 1.1 2011/04/24 00:35:22 martynas Exp $	*/
/* $NetBSD: fenv.c,v 1.3 2010/08/01 06:34:38 taca Exp $ */

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
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#include <assert.h>
#include <fenv.h>
#include <stddef.h>
#include <string.h>

#define _DIAGASSERT(x) ((void) 0) /* XXX */

/* Load x87 Control Word */
#define	__fldcw(__cw)		__asm__ __volatile__	\
	("fldcw %0" : : "m" (__cw))

/* No-Wait Store Control Word */
#define	__fnstcw(__cw)		__asm__ __volatile__	\
	("fnstcw %0" : "=m" (*(__cw)))

/* No-Wait Store Status Word */
#define	__fnstsw(__sw)		__asm__ __volatile__	\
	("fnstsw %0" : "=am" (*(__sw)))

/* No-Wait Clear Exception Flags */
#define	__fnclex()		__asm__ __volatile__	\
	("fnclex")

/* Load x87 Environment */
#define	__fldenv(__env)		__asm__ __volatile__	\
	("fldenv %0" : : "m" (__env))

/* No-Wait Store x87 environment */
#define	__fnstenv(__env)	__asm__ __volatile__	\
	("fnstenv %0" : "=m" (*(__env)))

/* Check for and handle pending unmasked x87 pending FPU exceptions */
#define	__fwait(__env)		__asm__	__volatile__	\
	("fwait")

/* Load the MXCSR register */
#define	__ldmxcsr(__mxcsr)	__asm__ __volatile__	\
	("ldmxcsr %0" : : "m" (__mxcsr))

/* Store the MXCSR register state */
#define	__stmxcsr(__mxcsr)	__asm__ __volatile__	\
	("stmxcsr %0" : "=m" (*(__mxcsr)))

/*
 * The following constant represents the default floating-point environment
 * (that is, the one installed at program startup) and has type pointer to
 * const-qualified fenv_t.
 *
 * It can be used as an argument to the functions within the <fenv.h> header
 * that manage the floating-point environment, namely fesetenv() and
 * feupdateenv().
 *
 * x87 fpu registers are 16bit wide. The upper bits, 31-16, are marked as
 * RESERVED. We provide a partial floating-point environment, where we
 * define only the lower bits. The reserved bits are extracted and set by the
 * consumers of FE_DFL_ENV, during runtime.
 */
fenv_t __fe_dfl_env = {
	{
		__INITIAL_NPXCW__,      /* Control word register */
		0x0,			/* Unused */
		0x0000,                 /* Status word register */
		0x0,			/* Unused */
		0x0000ffff,             /* Tag word register */
		0x0,			/* Unused */
		{
			0x0000, 0x0000,
			0x0000, 0xffff
		}
	},
	__INITIAL_MXCSR__		/* MXCSR register */
};

/*
 * Test for SSE support on this processor.
 *
 * We need to use ldmxcsr/stmxcsr to get correct results if any part
 * of the program was compiled to use SSE floating-point, but we can't
 * use SSE on older processors.
 *
 * In order to do so, we need to query the processor capabilities via the CPUID
 * instruction. We can make it even simpler though, by querying the machdep.sse
 * sysctl.
 */
static int __HAS_SSE = 0;

static void __test_sse(void) __attribute__ ((constructor));

static void __test_sse(void)
{
	size_t oldlen = sizeof(__HAS_SSE);
	int mib[2] = { CTL_MACHDEP, CPU_SSE };
	int rv;

	rv = sysctl(mib, 2, &__HAS_SSE, &oldlen, NULL, 0);
	if (rv == -1)
		__HAS_SSE = 0;
}

/*
 * The feclearexcept() function clears the supported floating-point exceptions
 * represented by `excepts'.
 */
int
feclearexcept(int excepts)
{
	fenv_t env;
	uint32_t mxcsr;
	int ex;

	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	ex = excepts & FE_ALL_EXCEPT;

	/* It's ~3x faster to call fnclex, than store/load fp env */
	if (ex == FE_ALL_EXCEPT) {
		__fnclex();
	} else {
		__fnstenv(&env);
		env.x87.status &= ~ex;
		__fldenv(env);
	}

	if (__HAS_SSE) {
		__stmxcsr(&mxcsr);
		mxcsr &= ~ex;
		__ldmxcsr(mxcsr);
	}

	/* Success */
	return (0);
}

/*
 * The fegetexceptflag() function stores an implementation-defined
 * representation of the states of the floating-point status flags indicated by
 * the argument excepts in the object pointed to by the argument flagp.
 */
int
fegetexceptflag(fexcept_t *flagp, int excepts)
{
	uint32_t mxcsr;
	uint16_t status;
	int ex;

	_DIAGASSERT(flagp != NULL);
	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	ex = excepts & FE_ALL_EXCEPT;

	__fnstsw(&status);
	if (__HAS_SSE)
		__stmxcsr(&mxcsr);
	else
		mxcsr = 0;

	*flagp = (mxcsr | status) & ex;

	/* Success */
	return (0);
}

/*
 * The feraiseexcept() function raises the supported floating-point exceptions
 * represented by the argument `excepts'.
 *
 * The standard explicitly allows us to execute an instruction that has the
 * exception as a side effect, but we choose to manipulate the status register
 * directly.
 *
 * The validation of input is being deferred to fesetexceptflag().
 */
int
feraiseexcept(int excepts)
{
	fexcept_t ex;

	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	ex = excepts & FE_ALL_EXCEPT;
	fesetexceptflag(&ex, excepts);
	__fwait();

	/* Success */
	return (0);
}

/*
 * This function sets the floating-point status flags indicated by the argument
 * `excepts' to the states stored in the object pointed to by `flagp'. It does
 * NOT raise any floating-point exceptions, but only sets the state of the flags.
 */
int
fesetexceptflag(const fexcept_t *flagp, int excepts)
{
	fenv_t env;
	uint32_t mxcsr;
	int ex;

	_DIAGASSERT(flagp != NULL);
	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	ex = excepts & FE_ALL_EXCEPT;

	__fnstenv(&env);
	env.x87.status &= ~ex;
	env.x87.status |= *flagp & ex;
	__fldenv(env);

	if (__HAS_SSE) {
		__stmxcsr(&mxcsr);
		mxcsr &= ~ex;
		mxcsr |= *flagp & ex;
		__ldmxcsr(mxcsr);
	}

	/* Success */
	return (0);
}

/*
 * The fetestexcept() function determines which of a specified subset of the
 * floating-point exception flags are currently set. The `excepts' argument
 * specifies the floating-point status flags to be queried.
 */
int
fetestexcept(int excepts)
{
	uint32_t mxcsr;
	uint16_t status;
	int ex;

	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	ex = excepts & FE_ALL_EXCEPT;

	__fnstsw(&status);
	if (__HAS_SSE)
		__stmxcsr(&mxcsr);
	else
		mxcsr = 0;

	return ((status | mxcsr) & ex);
}

int
fegetround(void)
{
	uint16_t control;

	/*
	 * We assume that the x87 and the SSE unit agree on the
	 * rounding mode.  Reading the control word on the x87 turns
	 * out to be about 5 times faster than reading it on the SSE
	 * unit on an Opteron 244.
	 */
	__fnstcw(&control);

	return (control & __X87_ROUND_MASK);
}

/*
 * The fesetround() function shall establish the rounding direction represented
 * by its argument round. If the argument is not equal to the value of a
 * rounding direction macro, the rounding direction is not changed.
 */
int
fesetround(int round)
{
	uint32_t mxcsr;
	uint16_t control;

	if (round & ~__X87_ROUND_MASK) {
		/* Failure */
		return (-1);
	}

	__fnstcw(&control);
	control &= ~__X87_ROUND_MASK;
	control |= round;
	__fldcw(control);

	if (__HAS_SSE) {
		__stmxcsr(&mxcsr);
		mxcsr &= ~(__X87_ROUND_MASK << __SSE_ROUND_SHIFT);
		mxcsr |= round << __SSE_ROUND_SHIFT;
		__ldmxcsr(mxcsr);
	}

	/* Success */
	return (0);
}

/*
 * The fegetenv() function attempts to store the current floating-point
 * environment in the object pointed to by envp.
 */
int
fegetenv(fenv_t *envp)
{
	uint32_t mxcsr;

	_DIAGASSERT(flagp != NULL);

	/*
	 * fnstenv masks all exceptions, so we need to restore the old control
	 * word to avoid this side effect.
	 */
	__fnstenv(envp);
	__fldcw(envp->x87.control);
	if (__HAS_SSE) {
		__stmxcsr(&mxcsr);
		envp->mxcsr = mxcsr;
	}

	/* Success */
	return (0);
}

/*
 * The feholdexcept() function saves the current floating-point environment in
 * the object pointed to by envp, clears the floating-point status flags, and
 * then installs a non-stop (continue on floating-point exceptions) mode, if
 * available, for all floating-point exceptions.
 */
int
feholdexcept(fenv_t *envp)
{
	uint32_t mxcsr;

	_DIAGASSERT(envp != NULL);

	__fnstenv(envp);
	__fnclex();
	if (__HAS_SSE) {
		__stmxcsr(&mxcsr);
		envp->mxcsr = mxcsr;
		mxcsr &= ~FE_ALL_EXCEPT;
		mxcsr |= FE_ALL_EXCEPT << __SSE_EMASK_SHIFT;
		__ldmxcsr(mxcsr);
	}

	/* Success */
	return (0);
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
	fenv_t env;

	_DIAGASSERT(envp != NULL);

	/* Store the x87 floating-point environment */
	memset(&env, 0, sizeof(env));
	__fnstenv(&env);

	__fe_dfl_env.x87.unused1 = env.x87.unused1;
	__fe_dfl_env.x87.unused2 = env.x87.unused2;
	__fe_dfl_env.x87.unused3 = env.x87.unused3;
	memcpy(__fe_dfl_env.x87.others,
	       env.x87.others,
	       sizeof(__fe_dfl_env.x87.others) / sizeof(uint32_t));

	__fldenv(envp->x87);
	if (__HAS_SSE)
		__ldmxcsr(envp->mxcsr);

	/* Success */
	return (0);
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
	fenv_t env;
	uint32_t mxcsr;
	uint16_t status;

	_DIAGASSERT(envp != NULL);

	/* Store the x87 floating-point environment */
	memset(&env, 0, sizeof(env));
	__fnstenv(&env);

	__fe_dfl_env.x87.unused1 = env.x87.unused1;
	__fe_dfl_env.x87.unused2 = env.x87.unused2;
	__fe_dfl_env.x87.unused3 = env.x87.unused3;
	memcpy(__fe_dfl_env.x87.others,
	       env.x87.others,
	       sizeof(__fe_dfl_env.x87.others) / sizeof(uint32_t));

	__fnstsw(&status);
	if (__HAS_SSE)
		__stmxcsr(&mxcsr);
	else
		mxcsr = 0;
	fesetenv(envp);
	feraiseexcept((mxcsr | status) & FE_ALL_EXCEPT);

	/* Success */
	return (0);
}

/*
 * The following functions are extentions to the standard
 */
int
feenableexcept(int mask)
{
	uint32_t mxcsr, omask;
	uint16_t control;

	mask &= FE_ALL_EXCEPT;
	__fnstcw(&control);
	if (__HAS_SSE)
		__stmxcsr(&mxcsr);
	else
		mxcsr = 0;

	omask = (control | mxcsr >> __SSE_EMASK_SHIFT) & FE_ALL_EXCEPT;
	control &= ~mask;
	__fldcw(control);
	if (__HAS_SSE) {
		mxcsr &= ~(mask << __SSE_EMASK_SHIFT);
		__ldmxcsr(mxcsr);
	}

	return (~omask);
}

int
fedisableexcept(int mask)
{
	uint32_t mxcsr, omask;
	uint16_t control;

	mask &= FE_ALL_EXCEPT;
	__fnstcw(&control);
	if (__HAS_SSE)
		__stmxcsr(&mxcsr);
	else
		mxcsr = 0;

	omask = (control | mxcsr >> __SSE_EMASK_SHIFT) & FE_ALL_EXCEPT;
	control |= mask;
	__fldcw(control);
	if (__HAS_SSE) {
		mxcsr |= mask << __SSE_EMASK_SHIFT;
		__ldmxcsr(mxcsr);
	}

	return (~omask);
}

int
fegetexcept(void)
{
	uint16_t control;

	/*
	 * We assume that the masks for the x87 and the SSE unit are
	 * the same.
	 */
	__fnstcw(&control);

	return (control & FE_ALL_EXCEPT);
}
