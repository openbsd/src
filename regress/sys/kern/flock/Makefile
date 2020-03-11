#	$OpenBSD: Makefile,v 1.10 2020/03/11 17:27:55 anton Exp $

PROGS+=		flock
SRCS_flock=	flock.c util.c

PROGS+=		lockf
SRCS_lockf=	lockf.c util.c

WARNINGS=	yes

TESTS_FLOCK!=	jot 24 1
.for t in ${TESTS_FLOCK}
flock-$t: flock
	@echo '\n======== $@ ========'
	./flock ${LOCKFLAGS} $t
REGRESS_TARGETS+=flock-$t
.endfor

TESTS_LOCKF!=	jot 2 1
.for t in ${TESTS_LOCKF}
lockf-$t: lockf
	@echo '\n======== $@ ========'
	./lockf ${LOCKFLAGS} $t
REGRESS_TARGETS+=lockf-$t
.endfor

.include <bsd.regress.mk>
