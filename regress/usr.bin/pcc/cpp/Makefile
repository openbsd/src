# $OpenBSD: Makefile,v 1.1 2007/10/07 18:12:43 otto Exp $

CC=/usr/local/bin/cc

regress:
	@echo skip001.c
	${CC} -E -I ${.CURDIR}/subdir ${.CURDIR}/skip001.c | cmp - ${.CURDIR}/skip001.out

.include <bsd.regress.mk>
