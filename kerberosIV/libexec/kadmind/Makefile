# $OpenBSD: Makefile,v 1.2 2000/03/01 08:54:59 deraadt Exp $

PROG	= kadmind

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/kadmin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl
LDADD	+= ${LIBROKEN} -lacl -lkdb -lkadm -lkrb -ldes -lcom_err

SRCS	= kadm_server.c kadm_funcs.c admin_server.c kadm_ser_wrap.c pw_check.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
