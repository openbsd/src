# $OpenBSD: Makefile,v 1.8 2002/05/11 00:20:19 espie Exp $

PROG	= ksrvutil

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/kadmin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/krb
DPADD	+= ${LIBKAFS} ${LIBKRB5} ${LIBKDB} ${LIBKADM} ${LIBKRB} ${LIBDES} \
	${LIBCRYPTO} ${LIBASN1} ${LIBCOM_ERR}
LDADD	+= -lkafs -lkrb5 -lkdb -lkadm -lkrb -ldes -lcrypto -lasn1 -lcom_err ${LIBROKEN}

SRCS	= ksrvutil.c ksrvutil_get.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
