#	$OpenBSD: Makefile,v 1.7 2002/05/06 22:23:53 deraadt Exp $

PROG=	rcp
SRCS=	rcp.c util.c
BINOWN=	root
BINMODE=4555
#INSTALLFLAGS=-fschg

.include <bsd.own.mk>	# For KERBEROS

.if (${KERBEROS:L} == "yes")
.PATH:	${.CURDIR}/../../usr.bin/rsh
SRCS+=	krcmd.c kcmd.c
CFLAGS+=-DKERBEROS
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
