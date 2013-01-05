/*	$OpenBSD: version.c,v 1.8 2013/01/05 11:20:56 miod Exp $ */

/*
 *	1.8	ELF toolchain
 *	1.7	compiled with gcc 3.3.5
 *	1.6	allocation area changed to fix netboot buffers overwriting stack
 *	1.5	rewritten crt code
 *	1.4	kernel loaded with loadfile, a.out and ELF formats
 *	1.3	rewritten startup code and general cleanup
 */
const char *version = "1.8";
