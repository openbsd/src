# $OpenBSD: Makefile,v 1.3 2001/01/09 02:41:22 angelos Exp $

PROG	= kdb_util

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -lkrb -ldes

SRCS	= kdb_util.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
