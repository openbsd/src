# $OpenBSD: Makefile,v 1.3 2001/01/09 02:41:19 angelos Exp $

PROG	= kpropd

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/slave

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkadm -lkdb -lkrb -ldes

SRCS	= kpropd.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
