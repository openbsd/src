#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.3 1997/04/19 21:26:20 millert Exp $

PROG=	rlogind
SRCS=	rlogind.c
MAN=	rlogind.8
DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.own.mk>	# For KERBEROS

.if (${KERBEROS} == "yes")
SRCS=	rlogind.c des_rw.c
.PATH:	${.CURDIR}/../../usr.bin/rlogin
CFLAGS+=-DKERBEROS
DPADD+=	${LIBKRB} ${LIBKRB}
LDADD+=	-lkrb -ldes
.endif

.include <bsd.prog.mk>
