# $OpenBSD: Makefile,v 1.2 2010/06/30 00:05:35 nicm Exp $
# $NetBSD: Makefile,v 1.5 2010/02/03 15:34:43 roy Exp $

NOMAN=1
PROG=tc1
CPPFLAGS=-I${.CURDIR}/..
LDADD+=-ledit -ltermcap
DPADD+=${LIBEDIT} ${LIBTERMCAP}

.include <bsd.prog.mk>
