#	$OpenBSD: Makefile,v 1.3 2017/06/02 20:03:54 otto Exp $

PROG=fenv

CFLAGS+=-ffloat-store
LDADD=-lm
DPADD=${LIBM}

.include <bsd.regress.mk>
