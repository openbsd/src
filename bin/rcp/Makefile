#	$OpenBSD: Makefile,v 1.4 1997/04/19 21:26:10 millert Exp $
#	$NetBSD: Makefile,v 1.6 1995/03/23 08:46:35 mycroft Exp $

PROG=	rcp
SRCS=	rcp.c util.c
BINOWN=	root
BINMODE=4555
#INSTALLFLAGS=-fschg

.include <bsd.own.mk>	# For KERBEROS

.if (${KERBEROS} == "yes")
.PATH:	${.CURDIR}/../../usr.bin/rlogin
SRCS+=	krcmd.c kcmd.c
CFLAGS+=-DKERBEROS
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
