#	$OpenBSD: Makefile,v 1.2 2004/03/09 22:23:41 miod Exp $

.if ${MACHINE} == "mvmeppc"
PROG=	installboot
MAN=	installboot.8
MANSUBDIR=/mvmeppc

BINDIR=/usr/mdec
DPADD=${LIBUTIL}
LDADD=-lutil
CFLAGS+=-O0
# Need this to work in the miniroot
LDSTATIC=-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>

