# $OpenBSD: Makefile,v 1.3 2001/01/09 02:41:23 angelos Exp $

PROG	= ksrvutil

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/kadmin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/krb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -lkadm -lkrb -ldes -lcom_err

SRCS	= ksrvutil.c ksrvutil_get.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
