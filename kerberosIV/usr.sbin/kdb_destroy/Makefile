# $OpenBSD: Makefile,v 1.2 2001/01/09 02:41:21 angelos Exp $

PROG	= kdb_destroy

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes -lkdb

SRCS	= kdb_destroy.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
