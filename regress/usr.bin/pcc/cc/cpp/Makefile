# $OpenBSD: Makefile,v 1.2 2007/10/05 16:45:22 otto Exp $

CC=/usr/local/bin/cc

regress:
	${CC} -E -I subdir skip001.c | cmp - ${.CURDIR}/skip001.out
