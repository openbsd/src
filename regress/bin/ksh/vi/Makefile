# $OpenBSD: Makefile,v 1.1 2016/01/12 09:00:39 schwarze Exp $
#
# Author: Ingo Schwarze <schwarze@openbsd.org>, 2016.  Public Domain.

REGRESS_TARGETS = check
PROG = test_vi
SRCS = vi.c test_vi.c
CPPFLAGS = -iquote ${.CURDIR}
CLEANFILES = vi.c

check test:
	sh ${.CURDIR}/vi.sh

vi.c: ${.CURDIR}/../../../../bin/ksh/vi.c
	cp ${.CURDIR}/../../../../bin/ksh/vi.c .

.include <bsd.regress.mk>
