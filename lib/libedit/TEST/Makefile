# $OpenBSD: Makefile,v 1.1 2003/11/25 20:12:39 otto Exp $
# $NetBSD: Makefile,v 1.1 2003/10/16 21:41:46 christos Exp $

NOMAN=1
PROG=test
CPPFLAGS=-I${.CURDIR}/..
LDADD+=-ledit -ltermcap
DPADD+=${LIBEDIT} ${LIBTERMCAP}

.include <bsd.prog.mk>
