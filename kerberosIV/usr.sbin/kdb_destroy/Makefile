# $OpenBSD: Makefile,v 1.4 2001/07/09 09:44:30 hin Exp $

PROG	= kdb_destroy

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN}

SRCS	= kdb_destroy.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
