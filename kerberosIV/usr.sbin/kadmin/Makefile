# $OpenBSD: Makefile,v 1.2 2000/03/01 08:55:07 deraadt Exp $

PROG	= kadmin

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/kadmin ${KRBSRC}/lib/sl

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl -I${KRBSRC}/lib/krb
LDADD	+= ${LIBROKEN} -lkafs -lkadm -lkrb -ldes -lcom_err -lreadline -lcurses

SRCS	= kadmin.c random_password.c new_pwd.c sl.c

MAN	= kadmin.8

.include <bsd.prog.mk>
