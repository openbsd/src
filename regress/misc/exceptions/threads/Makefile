#	$OpenBSD: Makefile,v 1.1 2021/02/20 19:05:28 otto Exp $

PROG=	exceptions
SRCS=	exceptions.cc

REGRESS_TARGETS=runs

runs: exceptions
	for i in $$(jot 100); do exceptions; done

.include <bsd.regress.mk>
