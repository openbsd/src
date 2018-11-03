#	$OpenBSD: Makefile,v 1.6 2018/11/03 09:21:25 anton Exp $

PROG=	flock

WARNINGS=	yes

TESTS!=jot 22 1

# XXX known failures (talk to art)
REGRESS_SKIP_TARGETS = t-5 t-6

.for a in ${TESTS}
t-${a}: flock
	./flock ${a}
REGRESS_TARGETS+=t-${a}
.endfor

.include <bsd.regress.mk>
