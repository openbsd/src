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
 *   Author :   Jeffrey Friedl
 *   Created:   July 1992
 *   Standalone crt0.
 */

/*
 * GCCisms used:
 * 	A "volatile void fcn()" is one that never returns.
 *	register var asm("r1"): variable VAR is raw access to named register.
 */

/*
 * When a program begins, r31 points to info passed from the kernel.
 *
 * The following shows the memory to which r31 points (think "int *r31;"),
 * and how we derive argc, argv, and envp from that:
 *
 *    +-------------------+ <-------------------------------------- r31
 *    | ARGC              | <- argc = r31[0];
 *    +-------------------+ <- argv = &r31[1];
 *    | &(argument #1)    |
 *    +-------------------+
 *    | &(argument #2)    |
 *     -  - - - - - - -  - 
 *    | &(argument #ARGC) |
 *    +-------------------+
 *    | 0x00000000        | <- end-of-ARGV-list marker (redundant information).
 *    +-------------------+ <- environ = envp =  &argv[argc+1];
 *    | &(env. var. #1)   |
 *    +-------------------+
 *    | &(env. var. #2)   |
 *     -  - - - - - - -  - 
 *    | &(env. var. #N)   |
 *    +-------------------+
 *    | 0x00000000        | <- end-of-ENVP-list marker (not redundant!).
 *    +-------------------+
 *
 * We use 'start:' to grab r31 and and call real_start(argc, argv, envp).
 * We must do this since the function prologue makes finding the initial
 * r31 difficult in C.
 */

#include <stdlib.h>

#include "common.h"

asm("       text                ");
asm("       align  4            ");
asm("start: global start        ");
asm("       ld     r2, r31,   0 "); /* First arg to real_start: argc */
asm("       addu   r3, r31,   4 "); /* Second arg to real_start: argv */
asm("       lda    r4,  r3  [r2]"); /* Third arg to real_start: envp, but.... */
asm("       addu   r4,  r4,   4 "); /*   ... don't forget to skip past marker */
asm("       br.n   ___crt0_real_start");
asm("       subu   r31, r31, 32 ");

#ifdef DYNAMIC
extern struct _dynamic	_DYNAMIC;
struct _dynamic	*___pdynamic = &_DYNAMIC;
#endif

/* static */ void volatile
__crt0_real_start(int argc, char *argv[], char *envp[])
{
	register char *ap;
	volatile int a = 0;
	extern int minbrk asm ("minbrk");
	extern int curbrk asm ("curbrk");
	extern int end;

	minbrk = (int)&end;
	curbrk = (int)&end;
	environ = envp; /* environ is for the user that can't get at 'envp' */

	if (ap = argv[0])
		if ((__progname = _strrchr(ap, '/')) == NULL)
			__progname = ap;
		else
			++__progname;
asm ("__callmain:");		/* Defined for the benefit of debuggers */
	exit(main(argc, argv, environ));

    /*NOTREACHED*/
}

#include "common.c"
