#	$OpenBSD: Makefile,v 1.2 2021/02/21 19:21:15 tb Exp $

PROG=	exceptions
SRCS=	exceptions.cc

REGRESS_TARGETS=runs

runs: exceptions
	for i in $$(jot 100); do ./exceptions; done

.include <bsd.regress.mk>
