/*	$OpenBSD: infinity.c,v 1.5 2008/12/09 19:52:34 martynas Exp $ */
/*
 * XXX - This is not correct, but what can we do about it?
 */

/* infinity.c */

#include <math.h>

/* The highest D float on a vax. */
char __infinity[] = { (char)0xff, (char)0x7f, (char)0xff, (char)0xff, 
	(char)0xff, (char)0xff, (char)0xff, (char)0xff };

/* The highest F float on a vax. */
char __infinityf[] = { (char)0xff, (char)0x7f, (char)0xff, (char)0xff };
