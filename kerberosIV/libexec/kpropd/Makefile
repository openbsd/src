# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:28 hin Exp $

PROG	= kpropd

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/slave

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkrb -lkadm -lkdb -ldes

SRCS	= kpropd.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
