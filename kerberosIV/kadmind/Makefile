#	$OpenBSD: Makefile,v 1.5 1997/02/19 04:02:20 tholo Exp $

.include <bsd.obj.mk>

PROG=   kadmind
SRCS=   admin_server.c kadm_funcs.c kadm_ser_wrap.c kadm_server.c
.if exists(${.CURDIR}/../kadm/${__objdir})
CFLAGS+=-I${.CURDIR}/../kadm/${__objdir}
.else
CFLAGS+=-I${.CURDIR}/../kadm
.endif
.if exists(${.CURDIR}/../krb/${__objdir})
CFLAGS+=-I${.CURDIR}/../krb/${__objdir}
.else
CFLAGS+=-I${.CURDIR}/../krb
.endif
CFLAGS+=-I${.CURDIR}/../../lib/libss
LDADD+=	-lkadm -lkdb -lkrb -ldes -lacl -lcom_err
DPADD=	${LIBKADM} ${LIBKDB} ${LIBKRB} ${LIBDES} ${LIBACL} ${LIBCOM_ERR}
MAN=	kadmind.8
BINDIR=/usr/sbin

.include <bsd.prog.mk>
