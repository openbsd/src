# $OpenBSD: Makefile,v 1.5 2002/05/11 00:20:18 espie Exp $

PROG	= afslog

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/appl/afsutil

CFLAGS	+= -I.
DPADD	+= ${LIBKAFS} ${LIBKRB5} ${LIBKRB} ${LIBDES} ${LIBCRYPTO} ${LIBASN1}
LDADD	+= -lkafs -lkrb5 -lkrb -ldes -lcrypto -lasn1 ${LIBROKEN}

SRCS	= aklog.c

.include <bsd.prog.mk>
