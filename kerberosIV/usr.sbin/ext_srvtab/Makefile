# $OpenBSD: Makefile,v 1.2 2000/03/01 08:55:07 deraadt Exp $

PROG	= ext_srvtab

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -lkrb -ldes

SRCS	= ext_srvtab.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
