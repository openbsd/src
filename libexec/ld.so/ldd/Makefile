#	$OpenBSD: Makefile,v 1.4 2013/02/17 22:06:31 deraadt Exp $

MAN=	ldd.1

.include <bsd.own.mk>

.if (${ELF_TOOLCHAIN:L} == "yes")
PROG=	ldd
SRCS=	ldd.c
#CFLAGS+=-Werror

BINDIR=	/usr/bin

.endif

.include <bsd.prog.mk>
