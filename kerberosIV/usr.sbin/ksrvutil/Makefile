# $OpenBSD: Makefile,v 1.7 2001/07/28 01:25:03 hugh Exp $

PROG	= ksrvutil

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/kadmin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/krb
LDADD	+= -lkafs -lkrb5 -lkdb -lkadm -lkrb -ldes -lcrypto -lasn1 -lcom_err ${LIBROKEN}

SRCS	= ksrvutil.c ksrvutil_get.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
