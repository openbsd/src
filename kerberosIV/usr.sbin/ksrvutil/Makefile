# $OpenBSD: Makefile,v 1.4 2001/07/01 21:47:19 deraadt Exp $

PROG	= ksrvutil

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/kadmin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/krb
LDADD	+= -lkafs -lkrb5 -lkrb -lkdb -lkadm -ldes -lcrypto -lasn1 -lcom_err ${LIBROKEN}

SRCS	= ksrvutil.c ksrvutil_get.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
