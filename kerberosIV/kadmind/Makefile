#	$Id: Makefile,v 1.1.1.1 1995/12/14 06:52:48 tholo Exp $

PROG=   kadmind
SRCS=   admin_server.c kadm_funcs.c kadm_ser_wrap.c kadm_server.c
.if exists(${.CURDIR}/../kadm/obj)
CFLAGS+=-I${.CURDIR}/../kadm/obj
.else
CFLAGS+=-I${.CURDIR}/../kadm
.endif
.if exists(${.CURDIR}/../krb/obj)
CFLAGS+=-I${.CURDIR}/../krb/obj
.else
CFLAGS+=-I${.CURDIR}/../krb
.endif
.if exists(${.CURDIR}/../ss/obj)
CFLAGS+=-I${.CURDIR}/../ss/obj
.else
CFLAGS+=-I${.CURDIR}/../ss
.endif
LDADD+=	-lkadm -lkdb -lkrb -ldes -lacl -lcom_err
DPADD=	${LIBKADM} ${LIBKDB} ${LIBKRB} ${LIBDES} ${LIBACL} ${LIBCOM_ERR}
MAN=	kadmind.8
BINDIR=/usr/libexec

.include <bsd.prog.mk>
