#	$OpenBSD: Makefile,v 1.3 1996/06/26 05:38:48 deraadt Exp $

PROG=	rsh
SRCS=	rsh.c
BINOWN=	root
BINMODE=4555

.include <bsd.own.mk>	# For KERBEROS

.if defined(KERBEROS)
SRCS+=	des_rw.c
.PATH:	${.CURDIR}/../rlogin
CFLAGS+=-DKERBEROS
SRCS+=	krcmd.c kcmd.c
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
