#	$OpenBSD: Makefile,v 1.6 2003/08/09 13:27:45 miod Exp $

MAN=	installboot.8
MANSUBDIR=mvme88k

.if ${MACHINE} == "mvme88k"
PROG=	installboot
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

