# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:35 hin Exp $

PROG	= kstash

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes -lkdb

SRCS	= kstash.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
