# $OpenBSD: Makefile,v 1.5 2001/07/09 09:44:31 hin Exp $

PROG	= ksrvutil

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/kadmin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/krb
LDADD	+= ${LIBROKEN} -lkadm -lkafs -lkrb -lkdb -ldes -lcom_err

SRCS	= ksrvutil.c ksrvutil_get.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
