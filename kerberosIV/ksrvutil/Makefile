#	$Id: Makefile,v 1.1.1.1 1995/12/14 06:52:53 tholo Exp $

PROG=	ksrvutil
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
LDADD+= -lkadm -lkrb -ldes -lcom_err
DPADD+=	${LIBKADM} ${LIBKRB} ${LIBDES} ${LIBCOM_ERR}
MAN=	ksrvutil.8

.include <bsd.prog.mk>
