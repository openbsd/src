# $OpenBSD: Makefile,v 1.4 2002/05/11 00:20:18 espie Exp $

PROG	= kadmind

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/kadmin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl
DPADD	+= ${LIBACL} ${LIBKDB} ${LIBKADM} ${LIBKRB} ${LIBDES} ${LIBCOM_ERR}
LDADD	+= ${LIBROKEN} -lacl -lkdb -lkadm -lkrb -ldes -lcom_err

SRCS	= kadm_server.c kadm_funcs.c admin_server.c kadm_ser_wrap.c pw_check.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
