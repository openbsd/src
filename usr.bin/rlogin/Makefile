#	$NetBSD: Makefile,v 1.5 1995/03/21 07:58:28 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 7/19/93

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
