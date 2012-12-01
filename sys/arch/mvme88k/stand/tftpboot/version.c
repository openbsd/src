/*	$OpenBSD: version.c,v 1.8 2012/12/01 21:08:50 miod Exp $ */

/*
 *	1.8	compiled with gcc 3.3.5
 *	1.7	recognize non-working NIOT configuration and ask the user to
 *		correct it instead of failing to boot silently
 *		tftp server
 *	1.6	allocation area changed to fix netboot buffers overwriting stack
 *	1.5	perform MVME197 busswitch initialization
 *	1.4	rewritten crt code, self-relocatable
 *	1.3	kernel loaded with loadfile, a.out and ELF formats
 *	1.2	rewritten startup code and general cleanup
 *	1.1	initial revision
 */
char *version = "1.8";
