#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:50:36 deraadt Exp $

PROG=	rlogin
SRCS=	rlogin.c
BINOWN=	root
BINMODE=4555
#INSTALLFLAGS=-fschg

.include <bsd.own.mk>

.if (${KERBEROS} == "yes")
CFLAGS+=-DKERBEROS
SRCS+=	krcmd.c kcmd.c des_rw.c
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
