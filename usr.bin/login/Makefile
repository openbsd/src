#	$NetBSD: Makefile,v 1.10 1994/12/23 06:52:56 jtc Exp $
#	@(#)Makefile	8.1 (Berkeley) 7/19/93

PROG=	login
SRCS=	login.c
DPADD=	${LIBUTIL} ${LIBCRYPT} ${LIBSKEY}
LDADD=	-lutil -lcrypt -lskey
CFLAGS+= -DSKEY

.if defined(KERBEROS5)
SRCS+=	k5login.c
DPADD+=	${LIBKRB5} ${LIBCRYPTO}
LDADD+=	-lkrb5 -lcrypto
CFLAGS+= -DKERBEROS5
.elif defined(KERBEROS)
SRCS+=	klogin.c
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkrb -ldes
CFLAGS+= -DKERBEROS
.endif

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
