#	$OpenBSD: Makefile,v 1.2 1996/06/02 14:42:03 niklas Exp $

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
.if exists(${.CURDIR}/../ss/${__objdir})
CFLAGS+=-I${.CURDIR}/../ss/${__objdir}
.else
CFLAGS+=-I${.CURDIR}/../ss
.endif
LDADD+= -lkadm -lkrb -ldes -lcom_err
DPADD+=	${LIBKADM} ${LIBKRB} ${LIBDES} ${LIBCOM_ERR}
MAN=	ksrvutil.8

.include <bsd.prog.mk>
