#	$OpenBSD: Makefile,v 1.5 2001/01/28 19:34:31 niklas Exp $

PROG=	rlogind
SRCS=	rlogind.c
MAN=	rlogind.8
DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.own.mk>	# For KERBEROS

.if (${KERBEROS:L} == "yes")
SRCS=	rlogind.c des_rw.c
.PATH:	${.CURDIR}/../../usr.bin/rlogin
CFLAGS+=-DKERBEROS
DPADD+=	${LIBKRB} ${LIBKRB}
LDADD+=	-lkrb -ldes
.endif

.include <bsd.prog.mk>
