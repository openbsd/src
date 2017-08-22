# $OpenBSD: Makefile,v 1.4 2017/08/22 20:14:57 anton Exp $

REGRESS_TARGETS=	emacs vi

KSH=	/bin/ksh

PROG=		edit
CFLAGS+=	-Wall
LDADD+=		-lutil

emacs: edit
	sh ${.CURDIR}/emacs.sh ${.CURDIR} ${KSH}

vi: edit
	sh ${.CURDIR}/vi.sh ${.CURDIR} ${KSH}

.include <bsd.regress.mk>
