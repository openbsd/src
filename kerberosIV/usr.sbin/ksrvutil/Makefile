# $OpenBSD: Makefile,v 1.2 2000/03/01 08:55:10 deraadt Exp $

PROG	= ksrvutil

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/kadmin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/krb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -lkadm -lkrb -ldes -lcom_err

SRCS	= ksrvutil.c ksrvutil_get.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
