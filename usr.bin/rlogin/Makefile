#	$OpenBSD: Makefile,v 1.3 1996/06/26 05:38:30 deraadt Exp $
#	$NetBSD: Makefile,v 1.5 1995/03/21 07:58:28 cgd Exp $

PROG=	rlogin
SRCS=	rlogin.c
BINOWN=	root
BINMODE=4555
#INSTALLFLAGS=-fschg

.include <bsd.own.mk>

.if defined(KERBEROS)
CFLAGS+=-DKERBEROS
SRCS+=	krcmd.c kcmd.c des_rw.c
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
