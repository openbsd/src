#	$OpenBSD: Makefile,v 1.9 1997/04/19 21:26:26 millert Exp $
#	$NetBSD: Makefile,v 1.10 1994/12/23 06:52:56 jtc Exp $

PROG=	login
SRCS=	login.c failedlogin.c
LDADD=	-lutil
DPADD=	${LIBUTIL}

.include <bsd.own.mk>	# For SKEY, KERBEROS and KERBEROS5

.if (${SKEY} == "yes")
CFLAGS+=-DSKEY
LDADD+=	-lskey
DPADD+=	${LIBSKEY}
.endif

.if (${KERBEROS5} == "yes")
CFLAGS+= -DKERBEROS5
SRCS+=	k5login.c
LDADD+=	-lkrb5 -lcrypto
DPADD+=	${LIBKRB5} ${LIBCRYPTO}
.elif (${KERBEROS} == "yes")
CFLAGS+= -DKERBEROS
SRCS+=	klogin.c
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
