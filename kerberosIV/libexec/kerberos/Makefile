# $OpenBSD: Makefile,v 1.3 2000/03/29 06:51:18 deraadt Exp $

PROG	= kerberos

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/server

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -lkrb -ldes

SRCS	= kerberos.c

MAN	= ${PROG}.8 ${PROG}.1

.include <bsd.prog.mk>
