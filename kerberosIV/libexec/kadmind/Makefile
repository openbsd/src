# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:27 hin Exp $

PROG	= kadmind

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/kadmin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl
LDADD	+= ${LIBROKEN} -lkrb -lkdb -lkadm -ldes -lacl -lcom_err

SRCS	= kadm_server.c kadm_funcs.c admin_server.c kadm_ser_wrap.c pw_check.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
