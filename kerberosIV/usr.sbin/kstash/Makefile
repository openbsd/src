# $OpenBSD: Makefile,v 1.5 2001/07/09 09:44:31 hin Exp $

PROG	= kstash

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -lkrb -ldes

SRCS	= kstash.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
