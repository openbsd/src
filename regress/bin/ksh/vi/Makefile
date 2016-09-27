# $OpenBSD: Makefile,v 1.2 2016/09/27 11:08:32 otto Exp $
#
# Author: Ingo Schwarze <schwarze@openbsd.org>, 2016.  Public Domain.

REGRESS_TARGETS = check
PROG = test_vi
SRCS = vi.c test_vi.c
CPPFLAGS = -iquote ${.CURDIR}
CLEANFILES = vi.c

check test: test_vi
	sh ${.CURDIR}/vi.sh

vi.c: ${.CURDIR}/../../../../bin/ksh/vi.c
	cp ${.CURDIR}/../../../../bin/ksh/vi.c .

.include <bsd.regress.mk>
