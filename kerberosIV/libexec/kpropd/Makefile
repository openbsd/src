# $OpenBSD: Makefile,v 1.4 2002/05/11 00:20:18 espie Exp $

PROG	= kpropd

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/slave

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl -I${KRBSRC}/lib/kdb
DPADD	+= ${LIBKADM} ${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD	+= ${LIBROKEN} -lkadm -lkdb -lkrb -ldes

SRCS	= kpropd.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
