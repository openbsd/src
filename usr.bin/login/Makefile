#	$OpenBSD: Makefile,v 1.6 1996/07/31 17:44:23 deraadt Exp $
#	$NetBSD: Makefile,v 1.10 1994/12/23 06:52:56 jtc Exp $

PROG=	login
SRCS=	login.c
#LDADD=	-lutil -lcrypt
LDADD=	-lutil
#DPADD=	${LIBUTIL} ${LIBCRYPT}
DPADD=	${LIBUTIL}

.include <bsd.own.mk>	# For SKEY, KERBEROS and KERBEROS5

.if defined(SKEY)
CFLAGS+=-DSKEY
LDADD+=	-lskey
DPADD+=	${LIBSKEY}
.endif

.if defined(KERBEROS5)
CFLAGS+= -DKERBEROS5
SRCS+=	k5login.c
LDADD+=	-lkrb5 -lcrypto
DPADD+=	${LIBKRB5} ${LIBCRYPTO}
.elif defined(KERBEROS)
CFLAGS+= -DKERBEROS
SRCS+=	klogin.c
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
