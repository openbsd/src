#	$OpenBSD: Makefile,v 1.6 2000/09/03 18:41:11 espie Exp $

PROG=	rcp
SRCS=	rcp.c util.c
BINOWN=	root
BINMODE=4555
#INSTALLFLAGS=-fschg

.include <bsd.own.mk>	# For KERBEROS

.if (${KERBEROS:L} == "yes")
.PATH:	${.CURDIR}/../../usr.bin/rlogin
SRCS+=	krcmd.c kcmd.c
CFLAGS+=-DKERBEROS
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
