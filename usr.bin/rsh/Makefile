#	$OpenBSD: Makefile,v 1.5 2000/09/03 18:41:18 espie Exp $

PROG=	rsh
SRCS=	rsh.c
BINOWN=	root
BINMODE=4555

.include <bsd.own.mk>	# For KERBEROS

.if (${KERBEROS:L} == "yes")
SRCS+=	des_rw.c
.PATH:	${.CURDIR}/../rlogin
CFLAGS+=-DKERBEROS
SRCS+=	krcmd.c kcmd.c
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
