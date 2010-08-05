# $OpenBSD: Makefile,v 1.4 2010/08/05 17:30:28 marco Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64")
PROG= acpidump
SRCS= acpidump.c
.else
NOPROG= yes
.endif

MAN= acpidump.8

VPATH=${.CURDIR}/aml
CFLAGS+=-I${.CURDIR}

BINDIR?=/usr/sbin

.include <bsd.prog.mk>
