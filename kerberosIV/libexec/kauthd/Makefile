# $OpenBSD: Makefile,v 1.7 2002/02/16 03:11:31 deraadt Exp $

PROG	= kauthd

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/appl/kauth

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl
LDADD	+= -lkafs -lkrb5 -lkrb -lkdb -lkadm -ldes -lcrypto ${LIBROKEN}

SRCS	= kauthd.c marshall.c encdata.c


MAN	= ${PROG}.8

.include <bsd.prog.mk>
