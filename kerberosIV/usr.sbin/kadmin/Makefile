# $OpenBSD: Makefile,v 1.4 2002/05/11 00:20:19 espie Exp $

PROG	= kadmin

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/kadmin ${KRBSRC}/lib/sl

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl -I${KRBSRC}/lib/krb
DPADD	+= ${LIBKAFS} ${LIBKADM} ${LIBKRB} ${LIBDES} ${LIBCOM_ERR} \
	${LIBREADLINE} ${LIBCURSES}
LDADD	+= ${LIBROKEN} -lkafs -lkadm -lkrb -ldes -lcom_err -lreadline -lcurses

SRCS	= kadmin.c random_password.c new_pwd.c sl.c

MAN	= kadmin.8

.include <bsd.prog.mk>
