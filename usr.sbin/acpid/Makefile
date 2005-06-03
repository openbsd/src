# $OpenBSD: Makefile,v 1.2 2005/06/03 17:55:24 deraadt Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64")
PROG=		acpid
SRCS=		main.c script.c
CFLAGS+=	-Wall -pedantic
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
.else
NOPROG=		yes
.endif

MAN=		acpid.8
MANSUBDIR=	i386 amd64

#.if make(install)
#SUBDIR+=	samples
#.endif

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
