# $OpenBSD: Makefile,v 1.2 2000/03/01 08:55:01 deraadt Exp $

PROG	= kerberos

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/server

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -lkrb -ldes

SRCS	= kerberos.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
