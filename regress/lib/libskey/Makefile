#       $OpenBSD: Makefile,v 1.2 2003/06/16 13:35:33 mickey Exp $

PROG=	skeytest
CFLAGS+=-Wall
LDADD+=	-lskey
DPADD+=	${LIBSKEY}

skey-regress: skeytest
	./${PROG}

REGRESS_TARGETS=skey-regress
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>

