# $OpenBSD: Makefile,v 1.4 2002/01/08 21:28:38 ericj Exp $

PROG=	modload
MAN=	modload.8

SRCS=	modload.c

.include <bsd.own.mk>	# for ELF_TOOLCHAIN

.if (${ELF_TOOLCHAIN:L} == "yes")
SRCS+=	elf.c
.else
SRCS+=	a.out.c
CFLAGS+=	-DUSE_AOUT
.endif

.include <bsd.prog.mk>
