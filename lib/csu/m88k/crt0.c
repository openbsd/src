/*	$OpenBSD: crt0.c,v 1.6 2003/12/26 00:29:11 miod Exp $	*/

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

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: crt0.c,v 1.6 2003/12/26 00:29:11 miod Exp $";
#endif /* LIBC_SCCS and not lint */

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

__asm__ ("
	.text
	.align 8
	.globl start
start:
	br.n	_start
	 or	r2, r31, r0
");

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

asm ("__callmain:");		/* Defined for the benefit of debuggers */
	exit(main(kfp->argc, argv, environ));
}

#include "common.c"
