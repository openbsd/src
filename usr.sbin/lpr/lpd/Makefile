#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$OpenBSD: Makefile,v 1.2 1997/01/17 16:12:39 millert Exp $

PROG=	lpd
CFLAGS+=-I${.CURDIR}/../common_source
MAN=	lpd.8
SRCS=	lpd.c printjob.c recvjob.c displayq.c rmjob.c startdaemon.c \
	lpdchar.c common.c key.c modes.c ttcompat.c
.PATH:	${.CURDIR}/../common_source

.include "../../Makefile.inc"
.include <bsd.prog.mk>
