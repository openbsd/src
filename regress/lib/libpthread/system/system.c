/*	$OpenBSD: system.c,v 1.1 2001/11/09 00:13:32 marc Exp $ */
/*
 *	Copyright (c) 2001 Marco S. Hyman
 *
 *	Permission to copy all or part of this material with or without
 *	modification for any purpose is granted provided that the above
 *	copyright notice and this paragraph are duplicated in all copies.
 *
 *	THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *	IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */ 

/*
 * system checks the threads system interface and that waitpid/wait4
 * works correctly.
 */

#include <stdlib.h>
#include "test.h"

int
main(int argc, char **argv)
{
    ASSERT(system("ls") == 0);
    SUCCEED;
}
