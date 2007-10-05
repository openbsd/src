# $OpenBSD: Makefile,v 1.3 2007/10/05 16:54:57 otto Exp $

CC=/usr/local/bin/cc

regress:
	@echo skip001.c
	${CC} -E -I ${.CURDIR}/subdir ${.CURDIR}/skip001.c | cmp - ${.CURDIR}/skip001.out

.include "bsd.regress.mk"
