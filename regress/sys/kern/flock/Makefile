#	$OpenBSD: Makefile,v 1.1 2008/07/25 11:41:22 art Exp $

PROG=	flock

TESTS!=jot 15 1

.for a in ${TESTS}
t-${a}:
	./flock /tmp ${a}
REGRESS_TARGETS+=t-${a}
.endfor

.include <bsd.regress.mk>
