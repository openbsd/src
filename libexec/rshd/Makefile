#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.2 1995/12/16 22:20:22 tholo Exp $

PROG=	rshd
SRCS=	rshd.c
MAN=	rshd.8

.include <bsd.own.mk>

.if defined(KERBEROS)
SRCS+=	des_rw.c
.PATH:	${.CURDIR}/../../usr.bin/rlogin
CFLAGS+=-DKERBEROS
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkrb -ldes
.endif

.include <bsd.prog.mk>
