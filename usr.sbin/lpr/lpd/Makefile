#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$OpenBSD: Makefile,v 1.5 2013/12/10 16:38:04 naddy Exp $

PROG=	lpd
CFLAGS+=-I${.CURDIR}/../common_source
MAN=	lpd.8
SRCS=	lpd.c printjob.c recvjob.c displayq.c rmjob.c startdaemon.c \
	lpdchar.c common.c key.c modes.c common_vars.c
BINGRP=	daemon
BINMODE=2550
.PATH:	${.CURDIR}/../common_source

.include "../../Makefile.inc"
.include <bsd.prog.mk>
