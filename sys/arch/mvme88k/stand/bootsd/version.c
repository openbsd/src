/*	$OpenBSD: version.c,v 1.9 2014/02/20 20:34:27 miod Exp $ */

/*
 *	1.9	/etc/random.seed support
 *	1.8	ELF toolchain
 *	1.7	compiled with gcc 3.3.5
 *	1.6	allocation area changed to fix netboot buffers overwriting stack
 *	1.5	rewritten crt code, self-relocatable
 *	1.4	kernel loaded with loadfile, a.out and ELF formats
 *	1.3	rewritten startup code and general cleanup
 */
const char *version = "1.9";
