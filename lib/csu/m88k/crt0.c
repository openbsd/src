/*	$OpenBSD: crt0.c,v 1.4 2003/12/25 23:26:09 miod Exp $	*/

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
static char rcsid[] = "$OpenBSD: crt0.c,v 1.4 2003/12/25 23:26:09 miod Exp $";
#endif /* LIBC_SCCS and not lint */

/*
 * When a program starts, r31 points to a structure passed from the kernel.
 *
 * This structure contains argc, the argv[] array, a NULL marker, then
 * the envp[] array, and another NULL marker.
 */

#include <stdlib.h>

#include "common.h"

extern void start(void) __asm__("start");

void
start(void)
{
	struct kframe {
		int	argc;
		char	*argv[0];
	};

	struct kframe *kfp;
	char **argv, *ap, *s;

	/*
	 * Pick the arguments frame as early as possible
	 */
	__asm__ __volatile__ ("or %0, r31, 0" : "=r" (kfp) :: "r31");

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
