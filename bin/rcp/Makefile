#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:35:37 deraadt Exp $

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
