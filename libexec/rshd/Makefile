#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.5 2000/09/03 18:41:15 espie Exp $

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
