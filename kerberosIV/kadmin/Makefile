#	$Id: Makefile,v 1.1.1.1 1995/12/14 06:52:48 tholo Exp $

PROG=	kadmin
SRCS=	kadmin.c kadmin_cmds.c maketime.c
.PATH:	${.CURDIR}/../kdb_edit
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
LDADD+=	-lkadm -lkrb -ldes -lss -lcom_err
DPADD+=	${LIBKADM} ${LIBKRB} ${LIBDES} ${LIBSS} ${LIBCOM_ERR}
CLEANFILES+= kadmin_cmds.c
MAN=	kadmin.8

kadmin_cmds.c:  kadmin_cmds.ct
	test -e kadmin_cmds.ct || ln -s ${.CURDIR}/kadmin_cmds.ct .
	${MK_CMDS} kadmin_cmds.ct
	-test -h kadmin_cmds.ct && rm kadmin_cmds.ct

.include <bsd.prog.mk>
