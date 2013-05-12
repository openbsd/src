/*	$OpenBSD: version.c,v 1.6 2013/05/12 08:10:07 miod Exp $ */

/*
 *	1.6	lower load address and heap location by 1MB to fit 8MB boards
 *	1.5	do not load kernel symbols to avoid seeking backwards
 *	1.4	kernel loaded with loadfile, a.out and ELF formats
 */

char *version = "1.6";
