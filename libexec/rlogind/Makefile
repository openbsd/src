#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.2 1995/12/16 22:20:19 tholo Exp $

PROG=	rlogind
SRCS=	rlogind.c
MAN=	rlogind.8
DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.own.mk>	# For KERBEROS

.if defined(KERBEROS)
SRCS=	rlogind.c des_rw.c
.PATH:	${.CURDIR}/../../usr.bin/rlogin
CFLAGS+=-DKERBEROS
DPADD+=	${LIBKRB} ${LIBKRB}
LDADD+=	-lkrb -ldes
.endif

.include <bsd.prog.mk>
