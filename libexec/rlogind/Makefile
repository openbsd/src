#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.4 2000/09/03 18:41:14 espie Exp $

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
