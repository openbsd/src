# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:32 hin Exp $

PROG	= kadmin

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/kadmin ${KRBSRC}/lib/sl

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl -I${KRBSRC}/lib/krb
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes -lkadm -lcom_err -lreadline -lcurses

SRCS	= kadmin.c random_password.c new_pwd.c sl.c

MAN	= kadmin.8

.include <bsd.prog.mk>
