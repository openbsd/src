#	$Id: Makefile,v 1.6 1997/12/16 20:59:46 deraadt Exp $

.include <bsd.obj.mk>

PROG=	kadmin
SRCS=	kadmin.c new_pwd.c sl.c
.PATH:	${.CURDIR}/../kdb_edit
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
DPADD+=	${LIBKADM} ${LIBKRB} ${LIBDES} ${LIBSS} ${LIBCOM_ERR}
CLEANFILES+= kadmin_cmds.c
MAN=	kadmin.8

.include <bsd.prog.mk>
