#	$OpenBSD: Makefile,v 1.8 2014/02/07 10:13:00 miod Exp $

MAN=	installboot.8
MANSUBDIR=mvme88k

.if ${MACHINE} == "mvme88k"
SRCS=	installboot.c nlist.c
PROG=	installboot
BINDIR=/usr/mdec
DPADD=${LIBUTIL}
LDADD=-lutil
# Need this to work in the miniroot
LDSTATIC=-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>

