# $OpenBSD: Makefile,v 1.5 2001/07/09 09:44:30 hin Exp $

PROG	= kdb_util

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -lkrb -ldes

SRCS	= kdb_util.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
