#	$Id: Makefile,v 1.7 1998/04/26 22:32:11 art Exp $

.include <bsd.obj.mk>

PROG=	kadmin
SRCS=	kadmin.c new_pwd.c sl.c

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

LDADD+=	-lkadm -lkrb -ldes -lcom_err
DPADD+=	${LIBKADM} ${LIBKRB} ${LIBDES} ${LIBCOM_ERR}
CLEANFILES+= kadmin_cmds.c
MAN=	kadmin.8

.include <bsd.prog.mk>
