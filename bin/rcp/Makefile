#	$OpenBSD: Makefile,v 1.10 2003/07/16 19:10:13 deraadt Exp $

PROG=	rcp
SRCS=	rcp.c util.c
BINOWN=	root
BINMODE=555
#INSTALLFLAGS=-fschg

.include <bsd.own.mk>	# For KERBEROS

#.if (${KERBEROS:L} == "yes")
#.PATH:	${.CURDIR}/../../usr.bin/rsh
#SRCS+=	krcmd.c kcmd.c
#CFLAGS+=-DKERBEROS
#DPADD+=	${LIBKRB} ${LIBDES}
#LDADD+=	-lkrb -ldes
#.endif

.include <bsd.prog.mk>
