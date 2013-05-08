/*	$OpenBSD: crt0.c,v 1.11 2013/05/08 16:06:45 miod Exp $	*/

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

#include <sys/param.h>
#include <stdlib.h>

char **environ;
char *__progname = "";

char __progname_storage[NAME_MAX + 1];

#ifdef MCRT0
extern void	monstartup(u_long, u_long);
extern void	_mcleanup(void);
extern unsigned char _etext, _eprol;
#endif /* MCRT0 */

static inline char *_strrchr(const char *p, char ch);

__asm__ (
"	.text\n"
"	.align 3\n"
"	.globl __start\n"
"	.globl _start\n"
"__start:\n"
"_start:\n"
"	or	%r0, %r0, %r0\n"	/* two nop because execution may */
"	or	%r0, %r0, %r0\n"	/* skip up to two instructions */
					/* see setregs() in the kernel */
					/* for details. */
"	ld	%r2, %r31, 0\n"		/* argc */
"	addu	%r3, %r31, 4\n"		/* argv */
"	lda	%r4, %r3[%r2]\n"
"	br.n	___start\n"
"	 addu	%r4, %r4, 4\n"		/* envp = argv + argc + 1 */
/* cleanup is %r5, zeroed in setregs() at the moment */
);

void
___start(int argc, char **argv, char **envp, void (*cleanup)(void))
{
	char *s;

	environ = envp;

	if ((__progname = argv[0]) != NULL) {	/* NULL ptr if argc = 0 */
		if ((__progname = _strrchr(__progname, '/')) == NULL)
			__progname = argv[0];
		else
			__progname++;
		for (s = __progname_storage; *__progname &&
		    s < &__progname_storage[sizeof __progname_storage - 1]; )
			*s++ = *__progname++;
		*s = '\0';
		__progname = __progname_storage;
	}

	if (cleanup)
		atexit(cleanup);

#ifdef MCRT0
	atexit(_mcleanup);
	monstartup((u_long)&_eprol, (u_long)&_etext);
#endif

	__init();

	exit(main(argc, argv, environ));
}

static char *
_strrchr(const char *p, char ch)
{
	char *save;

	for (save = NULL;; ++p) {
		if (*p == ch)
			save = (char *)p;
		if (!*p)
			return(save);
	}
}

#ifdef MCRT0
asm("\t.text\n_eprol:\n");
#endif
