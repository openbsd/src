# $OpenBSD: Makefile,v 1.8 2002/05/11 00:20:19 espie Exp $

PROG	= kdb_util

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
DPADD	+= ${LIBKAFS} ${LIBKRB5} ${LIBKDB} ${LIBKRB} ${LIBDES} ${LIBCRYPTO} \
	${LIBASN1}
LDADD	+= -lkafs -lkrb5 -lkdb -lkrb -ldes -lcrypto -lasn1 ${LIBROKEN}

SRCS	= kdb_util.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
