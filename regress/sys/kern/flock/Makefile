#	$OpenBSD: Makefile,v 1.7 2018/11/06 18:11:11 anton Exp $

PROGS+=		flock
SRCS_flock=	flock.c util.c

PROGS+=		lockf
SRCS_lockf=	lockf.c util.c

WARNINGS=	yes

TESTS_FLOCK!=	jot 22 1
.for t in ${TESTS_FLOCK}
flock-$t: flock
	./flock ${LOCKFLAGS} $t
REGRESS_TARGETS+=flock-$t
.endfor

# XXX known failures (talk to art)
REGRESS_SKIP_TARGETS=	flock-5 flock-6

TESTS_LOCKF!=	jot 2 1
.for t in ${TESTS_LOCKF}
lockf-$t: lockf
	./lockf ${LOCKFLAGS} $t
REGRESS_TARGETS+=lockf-$t
.endfor

.include <bsd.regress.mk>
