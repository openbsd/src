# $OpenBSD: Makefile,v 1.7 2001/07/28 01:25:03 hugh Exp $

PROG	= kstash

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= -lkafs -lkrb5 -lkdb -lkrb -ldes -lcrypto -lasn1 ${LIBROKEN}

SRCS	= kstash.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
