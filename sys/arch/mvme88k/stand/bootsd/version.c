/*	$OpenBSD: version.c,v 1.6 2009/01/18 21:49:11 miod Exp $ */

/*
 *	1.6	allocation area changed to fix netboot buffers overwriting stack
 *	1.5	rewritten crt code, self-relocatable
 *	1.4	kernel loaded with loadfile, a.out and ELF formats
 *	1.3	rewritten startup code and general cleanup
 */
char *version = "1.6";
