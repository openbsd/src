#	$OpenBSD: Makefile,v 1.1 2022/04/23 08:57:52 tobias Exp $

REGRESS_TARGETS= run-regress

LDFLAGS= -lutil

CLEANFILES= ibuf_test ibuf_test.d

run-regress: ibuf_test
	${.OBJDIR}/ibuf_test

.include <bsd.regress.mk>
