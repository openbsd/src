#	$OpenBSD: Makefile,v 1.7 2003/05/14 07:33:39 mho Exp $

PROG=	rsh
SRCS=	rsh.c
BINOWN=	root
BINMODE=4555

.include <bsd.own.mk>	# For KERBEROS

#.if (${KERBEROS:L} == "yes")
#SRCS+=	des_rw.c
#CFLAGS+=-DKERBEROS
#SRCS+=	krcmd.c kcmd.c
#LDADD+=	-lkrb -ldes
#DPADD+=	${LIBKRB} ${LIBDES}
#.endif

.include <bsd.prog.mk>
