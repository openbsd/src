/*	$OpenBSD: version.c,v 1.6 2009/01/18 21:49:11 miod Exp $ */

/*
 *	1.6	allocation area changed to fix netboot buffers overwriting stack
 *	1.5	perform MVME197 busswitch initialization
 *	1.4	rewritten crt code, self-relocatable
 *	1.3	kernel loaded with loadfile, a.out and ELF formats
 *	1.2	rewritten startup code and general cleanup
 *	1.1	initial revision
 */
char *version = "1.6";
