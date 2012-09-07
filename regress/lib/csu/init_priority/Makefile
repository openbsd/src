# $OpenBSD: Makefile,v 1.2 2012/09/07 05:01:20 miod Exp $

.include <bsd.own.mk>
.if ${COMPILER_VERSION} == "gcc2"
CFLAGS+=	-finit-priority
.endif

PROG=init_priority_test
SRCS=init_priority_test.cc

.include <bsd.regress.mk>
