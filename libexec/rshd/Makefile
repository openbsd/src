#	$OpenBSD: Makefile,v 1.8 2003/05/14 07:35:31 mho Exp $

PROG=	rshd
SRCS=	rshd.c
MAN=	rshd.8
CFLAGS+= -Wall -Wno-unused

.include <bsd.own.mk>

#.if (${KERBEROS:L} == "yes")
#SRCS+=	des_rw.c
#.PATH:	${.CURDIR}/../../usr.bin/rsh
#CFLAGS+=-DKERBEROS
#DPADD+=	${LIBKRB} ${LIBDES}
#LDADD+=	-lkrb -ldes
#.endif

.include <bsd.prog.mk>
