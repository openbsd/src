#	$OpenBSD: Makefile,v 1.7 2000/09/03 18:41:19 espie Exp $

PROG=	su
BINOWN=	root
BINMODE=4555

.include <bsd.own.mk>

.if (${SKEY:L} == "yes")
CFLAGS+=-DSKEY
LDADD+= -lskey
DPADD+= ${LIBSKEY}
.endif

.if (${KERBEROS:L} == "yes")
CFLAGS+=-DKERBEROS
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
