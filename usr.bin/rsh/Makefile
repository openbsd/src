#	$OpenBSD: Makefile,v 1.6 2002/05/06 22:23:53 deraadt Exp $

PROG=	rsh
SRCS=	rsh.c
BINOWN=	root
BINMODE=4555

.include <bsd.own.mk>	# For KERBEROS

.if (${KERBEROS:L} == "yes")
SRCS+=	des_rw.c
CFLAGS+=-DKERBEROS
SRCS+=	krcmd.c kcmd.c
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
