#	$OpenBSD: Makefile,v 1.2 1996/06/02 14:42:02 niklas Exp $

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
.if exists(${.CURDIR}/../ss/${__objdir})
CFLAGS+=-I${.CURDIR}/../ss/${__objdir}
.else
CFLAGS+=-I${.CURDIR}/../ss
.endif
LDADD+=	-lkadm -lkdb -lkrb -ldes -lacl -lcom_err
DPADD=	${LIBKADM} ${LIBKDB} ${LIBKRB} ${LIBDES} ${LIBACL} ${LIBCOM_ERR}
MAN=	kadmind.8
BINDIR=/usr/libexec

.include <bsd.prog.mk>
