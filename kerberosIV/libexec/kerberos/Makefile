# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:28 hin Exp $

PROG	= kerberos

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/server

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes -lkdb

SRCS	= kerberos.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
