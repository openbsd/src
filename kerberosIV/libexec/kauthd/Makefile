# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:27 hin Exp $

PROG	= kauthd

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/appl/kauth

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes

SRCS	= kauthd.c marshall.c encdata.c


MAN	= ${PROG}.8

.include <bsd.prog.mk>
