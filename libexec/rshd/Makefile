#	$OpenBSD: Makefile,v 1.7 2002/05/06 22:23:53 deraadt Exp $

PROG=	rshd
SRCS=	rshd.c
MAN=	rshd.8
CFLAGS+= -Wall -Wno-unused

.include <bsd.own.mk>

.if (${KERBEROS:L} == "yes")
SRCS+=	des_rw.c
.PATH:	${.CURDIR}/../../usr.bin/rsh
CFLAGS+=-DKERBEROS
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkrb -ldes
.endif

.include <bsd.prog.mk>
