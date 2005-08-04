/*	$OpenBSD: crt0.c,v 1.9 2005/08/04 16:33:05 espie Exp $	*/

/*   
 *   Mach Operating System
 *   Copyright (c) 1991, 1992 Carnegie Mellon University
 *   Copyright (c) 1991, 1992 Omron Corporation
 *   All Rights Reserved.
 *   
 *   Permission to use, copy, modify and distribute this software and its
 *   documentation is hereby granted, provided that both the copyright
 *   notice and this permission notice appear in all copies of the
 *   software, derivative works or modified versions, and any portions
 *   thereof, and that both notices appear in supporting documentation.
 *   
 *   CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 *   CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 *   ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *   
 *   Carnegie Mellon requests users of this software to return to
 *   
 *    Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *    School of Computer Science
 *    Carnegie Mellon University
 *    Pittsburgh PA 15213-3890
 *   
 *   any improvements or extensions that they make and grant Carnegie Mellon 
 *   the rights to redistribute these changes.
 */

/*
 * When a program begins, r31 points to a structure passed by the kernel.
 *
 * This structure contains argc, the argv[] NULL-terminated array, and
 * the envp[] NULL-terminated array.
 */

#include <stdlib.h>

#include "common.h"

struct kframe {
	int argc;
	char *argv[0];
};

__asm__ (
"	.text\n"
"	.align 8\n"
"	.globl start\n"
"start:\n"
"	br.n	_start\n"
"	 or	r2, r31, r0\n"
);

static void
start(struct kframe *kfp)
{
	char **argv, *ap, *s;

	argv = &kfp->argv[0];
	environ = argv + kfp->argc + 1;

	if (ap = argv[0]) {
		if ((__progname = _strrchr(ap, '/')) == NULL)
			__progname = ap;
		else
			++__progname;
		for (s = __progname_storage; *__progname &&
		    s < &__progname_storage[sizeof __progname_storage - 1]; )
			*s++ = *__progname++;
		*s = '\0';
		__progname = __progname_storage;
	}

__asm__ ("eprol:");

#ifdef MCRT0
	atexit(_mcleanup);
	monstartup((u_long)&eprol, (u_long)&etext);
#endif

__asm__ ("__callmain:");	/* Defined for the benefit of debuggers */
	exit(main(kfp->argc, argv, environ));
}

#include "common.c"

#ifdef MCRT0
__asm__ ("\
	text\n\
	global	_eprol\n\
_eprol:\n\
");
#endif
