# $OpenBSD: Makefile,v 1.6 2001/07/09 16:45:31 deraadt Exp $

PROG	= kdb_util

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= -lkafs -lkrb5 -lkrb -lkdb -ldes -lcrypto -lasn1 ${LIBROKEN}

SRCS	= kdb_util.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
