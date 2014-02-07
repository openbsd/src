#	$OpenBSD: Makefile,v 1.6 2014/02/07 10:13:00 miod Exp $

MAN=	installboot.8
MANSUBDIR=mvme68k

.if ${MACHINE} == "mvme68k"
SRCS=	installboot.c nlist.c
PROG=	installboot
BINDIR=/usr/mdec
DPADD=${LIBUTIL}
LDADD=-lutil
LDSTATIC=-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>
