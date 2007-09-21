# $OpenBSD: Makefile,v 1.1 2007/09/21 06:24:05 otto Exp $
regress:
	cc -E -I subdir skip001.c | cmp - ${.CURDIR}/skip001.out
