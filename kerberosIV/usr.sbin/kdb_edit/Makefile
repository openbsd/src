# $OpenBSD: Makefile,v 1.4 2001/07/01 21:47:16 deraadt Exp $

PROG	= kdb_edit

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= -lkafs -lkrb5 -lkrb -lkdb -ldes -lcrypto -lasn1 ${LIBROKEN}

SRCS	= kdb_edit.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
