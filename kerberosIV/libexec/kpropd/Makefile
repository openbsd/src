# $OpenBSD: Makefile,v 1.2 2000/03/01 08:55:01 deraadt Exp $

PROG	= kpropd

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/slave

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkadm -lkdb -lkrb -ldes

SRCS	= kpropd.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
