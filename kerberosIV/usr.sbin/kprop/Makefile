# $OpenBSD: Makefile,v 1.4 2001/07/01 21:47:18 deraadt Exp $

PROG	= kprop

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/slave ${KRBSRC}/lib/sl

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/krb -I${KRBSRC}/lib/kdb
LDADD	+= -lkafs -lkrb5 -lkrb -lkdb -lkadm -ldes -lasn1 -lcom_err \
	    -lacl -lcrypto ${LIBROKEN}

SRCS	= kprop.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
