#	$OpenBSD: Makefile,v 1.1 2001/06/26 21:58:03 smurph Exp $

.if ${MACHINE} == "mvmeppc"
PROG=	installboot
MAN=	installboot.8
MANSUBDIR=/mvme88k

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

