# $OpenBSD: Makefile,v 1.4 2001/01/09 02:41:19 angelos Exp $

PROG	= kerberos

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/server

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -lkrb -ldes

SRCS	= kerberos.c

MAN	= ${PROG}.8 ${PROG}.1

.include <bsd.prog.mk>
