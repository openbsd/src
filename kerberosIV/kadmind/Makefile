#	$OpenBSD: Makefile,v 1.8 1998/04/26 22:33:25 art Exp $

.include <bsd.obj.mk>

PROG=   kadmind
SRCS=   admin_server.c kadm_funcs.c kadm_ser_wrap.c kadm_server.c pw_check.c

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

LDADD+=	-lkadm -lkdb -lkrb -ldes -lacl -lcom_err
DPADD=	${LIBKADM} ${LIBKDB} ${LIBKRB} ${LIBDES} ${LIBACL} ${LIBCOM_ERR}
MAN=	kadmind.8
BINDIR=/usr/libexec

.include <bsd.prog.mk>
