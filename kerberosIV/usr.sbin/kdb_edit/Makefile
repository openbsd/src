# $OpenBSD: Makefile,v 1.3 2001/01/09 02:41:21 angelos Exp $

PROG	= kdb_edit

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -lkrb -ldes

SRCS	= kdb_edit.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
