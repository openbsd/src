#	$OpenBSD: Makefile,v 1.6 2001/01/28 19:34:32 niklas Exp $

PROG=	rshd
SRCS=	rshd.c
MAN=	rshd.8
CFLAGS+= -Wall -Wno-unused

.include <bsd.own.mk>

.if (${KERBEROS:L} == "yes")
SRCS+=	des_rw.c
.PATH:	${.CURDIR}/../../usr.bin/rlogin
CFLAGS+=-DKERBEROS
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkrb -ldes
.endif

.include <bsd.prog.mk>
