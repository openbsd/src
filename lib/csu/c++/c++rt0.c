/*	$OpenBSD: c++rt0.c,v 1.6 1999/02/01 17:02:47 pefo Exp $	*/
/*	$NetBSD: c++rt0.c,v 1.6 1997/12/29 15:36:50 pk Exp $	*/

/*
 * Copyright (c) 1993 Paul Kranenburg
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Run-time module for GNU C++ compiled shared libraries.
 *
 * The linker constructs the following arrays of pointers to global
 * constructors and destructors. The first element contains the
 * number of pointers in each.
 * The tables are also null-terminated.
 */
#include <stdlib.h>
#include <sys/exec.h>

#if !defined(NATIVE_EXEC_ELF)
/*
 * We make the __{C,D}TOR_LIST__ symbols appear as type `SETD' and
 * include a dummy local function in the set. This keeps references
 * to these symbols local to the shared object this module is linked to.
 */
static void dummy __P((void)) { return; }

/* Note: this is "a.out" dependent. */
__asm(".stabs \"___CTOR_LIST__\",22,0,0,_dummy");
__asm(".stabs \"___DTOR_LIST__\",22,0,0,_dummy");
#endif

extern void (*__CTOR_LIST__[]) __P((void));
extern void (*__DTOR_LIST__[]) __P((void));

static void	__dtors __P((void));
static void	__ctors __P((void));

static void
__dtors()
{
	unsigned long i = (unsigned long) __DTOR_LIST__[0];
	void (**p)(void) = __DTOR_LIST__ + i;
 
	while (i--)
		(**p--)();
}

static void
__ctors()
{
	void (**p)(void) = __CTOR_LIST__ + 1;

	while (*p)
		(**p++)();
}

#if !defined(NATIVE_EXEC_ELF)
extern void __init __P((void)) asm(".init");
extern void __fini __P((void)) asm(".fini");
#else
extern void __init __P((void)) __attribute__ ((section (".init")));
extern void __fini __P((void)) __attribute__ ((section (".fini")));
#endif

void
__init()
{
	static int initialized = 0;

	/*
	 * Call global constructors.
	 * Arrange to call global destructors at exit.
	 */
	if (!initialized) {
		initialized = 1;
		__ctors();
	}

}

void
__fini()
{
	/*
	 * Call global destructors.
	 */
	__dtors();
}
