# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:34 hin Exp $

PROG	= kprop

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/slave ${KRBSRC}/lib/sl

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/krb -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes -lkadm -lcom_err -lkdb -lacl

SRCS	= kprop.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
