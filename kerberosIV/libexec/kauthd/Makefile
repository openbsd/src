# $OpenBSD: Makefile,v 1.6 2001/07/09 16:45:25 deraadt Exp $

PROG	= kauthd

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/appl/kauth

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl
LDADD	+= -lkafs -lkrb5 -lkrb -lkdb -lkadm -ldes -lasn1 -lcom_err \
	    -lacl -lcrypto ${LIBROKEN}

SRCS	= kauthd.c marshall.c encdata.c


MAN	= ${PROG}.8

.include <bsd.prog.mk>
