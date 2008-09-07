/*	$OpenBSD: locks.c,v 1.1 2008/09/07 20:36:08 martynas Exp $	*/

/* Written by Martynas Venckus.  Public Domain. */

#include <stdio.h>

void *__dtoa_locks[] = { NULL, NULL };
