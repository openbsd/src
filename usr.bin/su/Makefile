#	$OpenBSD: Makefile,v 1.6 1997/04/19 21:26:30 millert Exp $

PROG=	su
BINOWN=	root
BINMODE=4555

.include <bsd.own.mk>

.if (${SKEY} == "yes")
CFLAGS+=-DSKEY
LDADD+= -lskey
DPADD+= ${LIBSKEY}
.endif

.if (${KERBEROS} == "yes")
CFLAGS+=-DKERBEROS
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
