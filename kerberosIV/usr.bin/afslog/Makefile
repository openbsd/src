# $OpenBSD: Makefile,v 1.4 2001/07/01 21:47:13 deraadt Exp $

PROG	= afslog

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/appl/afsutil

CFLAGS	+= -I.
LDADD	+= -lkafs -lkrb5 -lkrb -ldes -lcrypto -lasn1 ${LIBROKEN}

SRCS	= aklog.c

.include <bsd.prog.mk>
