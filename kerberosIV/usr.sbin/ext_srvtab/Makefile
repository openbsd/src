# $OpenBSD: Makefile,v 1.3 2001/01/09 02:41:20 angelos Exp $

PROG	= ext_srvtab

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -lkrb -ldes

SRCS	= ext_srvtab.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
