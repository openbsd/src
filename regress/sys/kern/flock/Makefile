#	$OpenBSD: Makefile,v 1.4 2018/10/16 16:54:18 anton Exp $

PROG=	flock

WARNINGS=	yes

TESTS!=jot 16 1

# XXX known failures (talk to art)
REGRESS_SKIP_TARGETS = t-5 t-6

.for a in ${TESTS}
t-${a}: flock
	./flock /tmp ${a}
REGRESS_TARGETS+=t-${a}
.endfor

.include <bsd.regress.mk>
