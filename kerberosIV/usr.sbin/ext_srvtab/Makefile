# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:32 hin Exp $

PROG	= ext_srvtab

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes -lkdb

SRCS	= ext_srvtab.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
