#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.3 1997/04/19 21:26:21 millert Exp $

PROG=	rshd
SRCS=	rshd.c
MAN=	rshd.8

.include <bsd.own.mk>

.if (${KERBEROS} == "yes")
SRCS+=	des_rw.c
.PATH:	${.CURDIR}/../../usr.bin/rlogin
CFLAGS+=-DKERBEROS
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkrb -ldes
.endif

.include <bsd.prog.mk>
