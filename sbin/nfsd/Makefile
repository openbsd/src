#	$OpenBSD: Makefile,v 1.6 2000/09/03 18:41:16 espie Exp $

PROG=	nfsd
MAN=	nfsd.8

.include <bsd.own.mk>	# For KERBEROS

.if (${KERBEROS:L} == "yes")
CFLAGS+=-DKERBEROS
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
