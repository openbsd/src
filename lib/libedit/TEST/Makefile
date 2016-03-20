# $OpenBSD: Makefile,v 1.3 2016/03/20 22:20:17 schwarze Exp $
# $NetBSD: Makefile,v 1.5 2010/02/03 15:34:43 roy Exp $

WIDECHAR=	yes

NOMAN=1
PROG=tc1
CPPFLAGS=-I${.CURDIR}/..
LDADD+=-ledit -ltermcap
DPADD+=${LIBEDIT} ${LIBTERMCAP}

.if "${WIDECHAR}" == "yes"
CPPFLAGS+=-DWIDECHAR
.endif

.include <bsd.prog.mk>
