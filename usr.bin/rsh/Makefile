#	$OpenBSD: Makefile,v 1.4 1997/04/19 21:26:29 millert Exp $

PROG=	rsh
SRCS=	rsh.c
BINOWN=	root
BINMODE=4555

.include <bsd.own.mk>	# For KERBEROS

.if (${KERBEROS} == "yes")
SRCS+=	des_rw.c
.PATH:	${.CURDIR}/../rlogin
CFLAGS+=-DKERBEROS
SRCS+=	krcmd.c kcmd.c
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
