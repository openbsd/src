# $OpenBSD: Makefile,v 1.2 2000/03/01 08:55:09 deraadt Exp $

PROG	= kprop

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/slave ${KRBSRC}/lib/sl

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/krb -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -lkadm -lkrb -ldes -lcom_err -lacl

SRCS	= kprop.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
