# $OpenBSD: Makefile,v 1.6 2001/07/09 09:44:30 hin Exp $

PROG	= kerberos

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/server

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -lkrb -lkadm -ldes -lasn1 -lcom_err \
	    -lacl

SRCS	= kerberos.c

MAN	= ${PROG}.8 ${PROG}.1

.include <bsd.prog.mk>
