#	$OpenBSD: Makefile,v 1.2 2016/10/26 11:24:17 otto Exp $

PROG=fenv

CFLAGS=-ffloat-store
LDADD=-lm
DPADD=${LIBM}

.include <bsd.regress.mk>
