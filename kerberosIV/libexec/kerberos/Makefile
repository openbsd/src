# $OpenBSD: Makefile,v 1.5 2001/07/01 21:47:09 deraadt Exp $

PROG	= kerberos

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/server

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl -I${KRBSRC}/lib/kdb
LDADD	+= -lkafs -lkrb5 -lkrb -lkdb -lkadm -ldes -lasn1 -lcom_err \
	    -lacl -lcrypto ${LIBROKEN}

SRCS	= kerberos.c

MAN	= ${PROG}.8 ${PROG}.1

.include <bsd.prog.mk>
