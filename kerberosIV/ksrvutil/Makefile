#	$OpenBSD: Makefile,v 1.4 1997/02/11 04:40:39 tholo Exp $

.include <bsd.obj.mk>

PROG=	ksrvutil
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
#CFLAGS+=-I${.CURDIR}/../../lib/libss
LDADD+= -lkadm -lkrb -ldes -lcom_err
DPADD+=	${LIBKADM} ${LIBKRB} ${LIBDES} ${LIBCOM_ERR}
MAN=	ksrvutil.8

.include <bsd.prog.mk>
