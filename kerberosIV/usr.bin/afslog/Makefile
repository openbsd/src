# $OpenBSD: Makefile,v 1.6 2002/06/10 18:30:05 hin Exp $

PROG	= afslog

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/appl/afsutil

CFLAGS	+= -I. -I/usr/include/kerberosV
DPADD	+= ${LIBKAFS} ${LIBKRB5} ${LIBKRB} ${LIBDES} ${LIBCRYPTO} ${LIBASN1}
LDADD	+= -lkafs -lkrb5 -lkrb -ldes -lcrypto -lasn1 ${LIBROKEN}

SRCS	= aklog.c

.include <bsd.prog.mk>
