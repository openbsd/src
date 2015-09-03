# $OpenBSD: Makefile,v 1.5 2015/09/03 11:30:13 yasuoka Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64")
PROG= acpidump
SRCS= acpidump.c
LDADD=	-lkvm
DPADD=	${LIBKVM}
.else
NOPROG= yes
.endif

MAN= acpidump.8

VPATH=${.CURDIR}/aml
CFLAGS+=-I${.CURDIR}

BINDIR?=/usr/sbin

.include <bsd.prog.mk>
