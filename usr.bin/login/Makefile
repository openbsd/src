#	$OpenBSD: Makefile,v 1.15 2000/09/03 18:41:18 espie Exp $

PROG=	login
SRCS=	login.c failedlogin.c
LDADD=	-lutil
DPADD=	${LIBUTIL}
CFLAGS+= -Wall -Wno-unused

.include <bsd.own.mk>	# For SKEY, KERBEROS and KERBEROS5

.if (${SKEY:L} == "yes")
CFLAGS+=-DSKEY
LDADD+=	-lskey
DPADD+=	${LIBSKEY}
.endif

.if (${KERBEROS5:L} == "yes")
CFLAGS+= -DKERBEROS5
SRCS+=	k5login.c
LDADD+=	-lkrb5 -lcrypto
DPADD+=	${LIBKRB5} ${LIBCRYPTO}
.elif (${KERBEROS:L} == "yes")
CFLAGS+= -DKERBEROS
SRCS+=	klogin.c
LDADD+=	-lkafs -lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES} ${LIBKAFS}
.endif

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
