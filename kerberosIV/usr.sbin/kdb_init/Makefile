# $OpenBSD: Makefile,v 1.2 2000/03/01 08:55:08 deraadt Exp $

PROG	= kdb_init

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -ldes -lkrb

SRCS	= kdb_init.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
