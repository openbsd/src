#	$Id: Makefile,v 1.2 1996/06/02 14:42:01 niklas Exp $

PROG=	kadmin
SRCS=	kadmin.c kadmin_cmds.c maketime.c
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
.if exists(${.CURDIR}/../ss/${__objdir})
CFLAGS+=-I${.CURDIR}/../ss/${__objdir}
.else
CFLAGS+=-I${.CURDIR}/../ss
.endif
LDADD+=	-lkadm -lkrb -ldes -lss -lcom_err
DPADD+=	${LIBKADM} ${LIBKRB} ${LIBDES} ${LIBSS} ${LIBCOM_ERR}
CLEANFILES+= kadmin_cmds.c
MAN=	kadmin.8

kadmin_cmds.c:  kadmin_cmds.ct
	test -e kadmin_cmds.ct || ln -s ${.CURDIR}/kadmin_cmds.ct .
	${MK_CMDS} kadmin_cmds.ct
	-test -h kadmin_cmds.ct && rm kadmin_cmds.ct

.include <bsd.prog.mk>
