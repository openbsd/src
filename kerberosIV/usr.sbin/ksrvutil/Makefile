# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:35 hin Exp $

PROG	= ksrvutil

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/kadmin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/krb
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes -lkdb -lkadm -lcom_err

SRCS	= ksrvutil.c ksrvutil_get.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
