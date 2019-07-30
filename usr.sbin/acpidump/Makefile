# $OpenBSD: Makefile,v 1.6 2017/01/24 00:20:28 deraadt Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64")
PROG= acpidump
SRCS= acpidump.c
LDADD=	-lkvm
DPADD=	${LIBKVM}
.else
NOPROG= yes
.endif

MAN= acpidump.8

.include <bsd.prog.mk>
