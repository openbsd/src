# $OpenBSD: Makefile,v 1.9 2002/05/11 00:20:18 espie Exp $

PROG	= kerberos

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/server

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl -I${KRBSRC}/lib/kdb
DPADD	+= ${LIBKAFS} ${LIBKRB5} ${LIBKDB} ${LIBKRB} ${LIBKADM} ${LIBDES} \
	${LIBASN1} ${LIBCOM_ERR} ${LIBACL} ${LIBCRYPTO}
LDADD	+= -lkafs -lkrb5 -lkdb -lkrb -lkadm -ldes -lasn1 -lcom_err \
	    -lacl -lcrypto ${LIBROKEN}

SRCS	= kerberos.c

MAN	= ${PROG}.8 ${PROG}.1

.include <bsd.prog.mk>
