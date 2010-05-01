/*	$OpenBSD: crtbegin.c,v 1.14 2010/05/01 11:32:43 kettenis Exp $	*/
/*	$NetBSD: crtbegin.c,v 1.1 1996/09/12 16:59:03 cgd Exp $	*/

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

#include "md_init.h"
#include "os-note-elf.h"
#include "extern.h"

struct dwarf2_eh_object {
	void *space[8];
};

void __register_frame_info(const void *, struct dwarf2_eh_object *)
    __attribute__((weak));

void __register_frame_info(const void *begin, struct dwarf2_eh_object *ob)
{
}

static const char __EH_FRAME_BEGIN__[]
    __attribute__((section(".eh_frame"), aligned(4))) = { };

/*
 * java class registration hooks
 */

#if (__GNUC__ > 2)
static void *__JCR_LIST__[]
    __attribute__((section(".jcr"), aligned(sizeof(void*)))) = { };

extern void _Jv_RegisterClasses (void *)
    __attribute__((weak));
#endif

/*
 * Include support for the __cxa_atexit/__cxa_finalize C++ abi for
 * gcc > 2.x. __dso_handle is NULL in the main program and a unique
 * value for each C++ shared library. For more info on this API, see:
 *
 *     http://www.codesourcery.com/cxx-abi/abi.html#dso-dtor
 */

#if (__GNUC__ > 2)
void *__dso_handle = NULL;
__asm(".hidden  __dso_handle");
#endif

static const init_f __CTOR_LIST__[1]
    __attribute__((section(".ctors"))) = { (void *)-1 };	/* XXX */
static const init_f __DTOR_LIST__[1]
    __attribute__((section(".dtors"))) = { (void *)-1 };	/* XXX */

static void	__dtors(void) __used;
static void	__ctors(void) __used;

static void
__dtors()
{
	unsigned long i = (unsigned long) __DTOR_LIST__[0];
	const init_f *p;

	if (i == -1)  {
		for (i = 1; __DTOR_LIST__[i] != NULL; i++)
			;
		i--;
	}
	p = __DTOR_LIST__ + i;
	while (i--)
		(**p--)();
}

static void
__ctors()
{
	const init_f *p = __CTOR_LIST__ + 1;

	while (*p)
		(**p++)();
}

void __init(void);
void __fini(void);
static void __do_init(void) __used;
static void __do_fini(void) __used;

MD_SECTION_PROLOGUE(".init", __init);

MD_SECTION_PROLOGUE(".fini", __fini);

MD_SECT_CALL_FUNC(".init", __do_init);
MD_SECT_CALL_FUNC(".fini", __do_fini);


void
__do_init()
{
	static int initialized = 0;
	static struct dwarf2_eh_object object;

	/*
	 * Call global constructors.
	 * Arrange to call global destructors at exit.
	 */
	if (!initialized) {
		initialized = 1;

		__register_frame_info(__EH_FRAME_BEGIN__, &object);

#if (__GNUC__ > 2)
		if (__JCR_LIST__[0] && _Jv_RegisterClasses)
			_Jv_RegisterClasses(__JCR_LIST__);
#endif

		(__ctors)();

		atexit(__fini);
	}
}

void
__do_fini()
{
	static int finalized = 0;

	if (!finalized) {
		finalized = 1;
		/*
		 * Call global destructors.
		 */
		(__dtors)();
	}
}

