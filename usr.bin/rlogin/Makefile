#	$OpenBSD: Makefile,v 1.4 1997/04/19 21:26:28 millert Exp $
#	$NetBSD: Makefile,v 1.5 1995/03/21 07:58:28 cgd Exp $

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
