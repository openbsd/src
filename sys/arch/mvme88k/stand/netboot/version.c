/*	$OpenBSD: version.c,v 1.7 2009/01/18 21:49:11 miod Exp $ */

/*
 *	1.7	allocation area changed to fix netboot buffers overwriting stack
 *	1.6	perform MVME197 busswitch initialization
 *	1.5	rewritten crt code, self-relocatable
 *	1.4	kernel loaded with loadfile, a.out and ELF formats
 *	1.3	rewritten startup code and general cleanup
 */
char *version = "1.7";
