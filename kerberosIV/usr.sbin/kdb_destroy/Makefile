# $OpenBSD: Makefile,v 1.3 2001/07/01 21:47:15 deraadt Exp $

PROG	= kdb_destroy

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= -lkafs -lkrb5 -lkrb -lkdb -ldes -lcrypto -lasn1 ${LIBROKEN}

SRCS	= kdb_destroy.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
