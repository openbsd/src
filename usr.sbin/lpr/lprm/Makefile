#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$OpenBSD: Makefile,v 1.5 2002/06/11 22:47:07 millert Exp $

PROG=	lprm
CFLAGS+=-I${.CURDIR}/../common_source
SRCS=	lprm.c rmjob.c startdaemon.c common.c common_vars.c
BINOWN=	daemon
BINGRP=	daemon
BINMODE=6555
BINDIR=	/usr/bin
.PATH:	${.CURDIR}/../common_source

.include <bsd.prog.mk>
