# $OpenBSD: Makefile,v 1.4 2002/05/11 00:20:19 espie Exp $

PROG	= pagsh

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/appl/afsutil

CFLAGS	+= -I.
DPADD	+= ${LIBKRB} ${LIBKAFS} ${LIBCRYPTO}
LDADD	+= ${LIBROKEN} -lkrb -lkafs -lcrypto

SRCS	= pagsh.c

.include <bsd.prog.mk>
