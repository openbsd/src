#	$OpenBSD: Makefile,v 1.6 2000/09/03 18:41:18 espie Exp $

PROG=	rlogin
SRCS=	rlogin.c
BINOWN=	root
BINMODE=4555
#INSTALLFLAGS=-fschg

.include <bsd.own.mk>

.if (${KERBEROS:L} == "yes")
CFLAGS+=-DKERBEROS
SRCS+=	krcmd.c kcmd.c des_rw.c
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
