/*	$OpenBSD: infinity.c,v 1.4 2005/08/07 16:40:15 espie Exp $ */
/*
 * XXX - This is not correct, but what can we do about it?
 */

/* infinity.c */

#include <math.h>

/* The highest D float on a vax. */
char __infinity[] = { (char)0xff, (char)0x7f, (char)0xff, (char)0xff, 
	(char)0xff, (char)0xff, (char)0xff, (char)0xff };
