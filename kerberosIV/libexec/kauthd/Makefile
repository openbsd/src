# $OpenBSD: Makefile,v 1.5 2001/07/09 09:44:29 hin Exp $

PROG	= kauthd

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/appl/kauth

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl
LDADD	+= ${LIBROKEN} -lkrb -ldes

SRCS	= kauthd.c marshall.c encdata.c


MAN	= ${PROG}.8

.include <bsd.prog.mk>
