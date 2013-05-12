/*	$OpenBSD: version.c,v 1.10 2013/05/12 10:43:45 miod Exp $ */

/*
 *	1.10	MVME376 support
 *	1.9	ELF toolchain
 *	1.8	compiled with gcc 3.3.5
 *	1.7	allocation area changed to fix netboot buffers overwriting stack
 *	1.6	perform MVME197 busswitch initialization
 *	1.5	rewritten crt code, self-relocatable
 *	1.4	kernel loaded with loadfile, a.out and ELF formats
 *	1.3	rewritten startup code and general cleanup
 */
const char *version = "1.10";
