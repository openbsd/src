# $OpenBSD: Makefile,v 1.8 2001/07/28 01:25:02 hugh Exp $

PROG	= kerberos

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/server

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl -I${KRBSRC}/lib/kdb
LDADD	+= -lkafs -lkrb5 -lkdb -lkrb -lkadm -ldes -lasn1 -lcom_err \
	    -lacl -lcrypto ${LIBROKEN}

SRCS	= kerberos.c

MAN	= ${PROG}.8 ${PROG}.1

.include <bsd.prog.mk>
