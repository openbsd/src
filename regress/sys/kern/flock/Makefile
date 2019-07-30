#	$OpenBSD: Makefile,v 1.3 2011/07/07 01:38:53 guenther Exp $

PROG=	flock

TESTS!=jot 15 1

# XXX known failures (talk to art)
REGRESS_SKIP_TARGETS = t-5 t-6

.for a in ${TESTS}
t-${a}: flock
	./flock /tmp ${a}
REGRESS_TARGETS+=t-${a}
.endfor

.include <bsd.regress.mk>
