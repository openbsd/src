#	$OpenBSD: Makefile,v 1.11 1998/03/26 20:28:04 art Exp $

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
LDADD+=	-lkrb -ldes -lkafs
DPADD+=	${LIBKRB} ${LIBDES}
.endif

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
