# $OpenBSD: Makefile,v 1.9 2002/05/11 00:20:18 espie Exp $

PROG	= kauthd

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/appl/kauth

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl
DPADD	+= ${LIBKAFS} ${LIBKRB} ${LIBKDB} ${LIBKADM} ${LIBKRB5} ${LIBASN1} \
	${LIBDES} ${LIBCRYPTO}
LDADD	+= -lkafs -lkrb -lkdb -lkadm -lkrb5 -lasn1 -ldes -lcrypto ${LIBROKEN}

SRCS	= kauthd.c marshall.c encdata.c


MAN	= ${PROG}.8

.include <bsd.prog.mk>
