#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.4 1998/05/15 04:28:25 art Exp $

PROG=	rshd
SRCS=	rshd.c
MAN=	rshd.8
CFLAGS+= -Wall -Wno-unused

.include <bsd.own.mk>

.if (${KERBEROS} == "yes")
SRCS+=	des_rw.c
.PATH:	${.CURDIR}/../../usr.bin/rlogin
CFLAGS+=-DKERBEROS
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkrb -ldes
.endif

.include <bsd.prog.mk>
