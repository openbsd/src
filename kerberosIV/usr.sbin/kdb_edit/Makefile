# $OpenBSD: Makefile,v 1.5 2002/05/11 00:20:19 espie Exp $

PROG	= kdb_edit

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
DPADD	+= ${LIBKAFS} ${LIBKRB5} ${LIBKRB} ${LIBKDB} ${LIBDES} ${LIBCRYPTO} \
	${LIBASN1}
LDADD	+= -lkafs -lkrb5 -lkrb -lkdb -ldes -lcrypto -lasn1 ${LIBROKEN}

SRCS	= kdb_edit.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
