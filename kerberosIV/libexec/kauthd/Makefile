# $OpenBSD: Makefile,v 1.3 2001/01/09 02:41:18 angelos Exp $

PROG	= kauthd

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/appl/kauth

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl
LDADD	+= ${LIBROKEN} -lkafs -lkrb -ldes

SRCS	= kauthd.c marshall.c encdata.c


MAN	= ${PROG}.8

.include <bsd.prog.mk>
