# $OpenBSD: Makefile,v 1.5 2001/07/09 09:44:30 hin Exp $

PROG	= ext_srvtab

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -lkrb -lcrypto

SRCS	= ext_srvtab.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
