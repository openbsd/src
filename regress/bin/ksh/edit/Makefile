# $OpenBSD: Makefile,v 1.3 2017/07/06 19:40:18 bluhm Exp $

REGRESS_TARGETS=	emacs vi

PROG=		edit
CFLAGS+=	-Wall
LDADD+=		-lutil

emacs: edit
	sh ${.CURDIR}/emacs.sh ${.CURDIR}

vi: edit
	sh ${.CURDIR}/vi.sh ${.CURDIR}

.include <bsd.regress.mk>
