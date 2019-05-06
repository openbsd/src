#	$OpenBSD: Makefile,v 1.9 2019/05/06 16:32:04 anton Exp $

PROGS+=		flock
SRCS_flock=	flock.c util.c

PROGS+=		lockf
SRCS_lockf=	lockf.c util.c

WARNINGS=	yes

TESTS_FLOCK!=	jot 24 1
.for t in ${TESTS_FLOCK}
flock-$t: flock
	./flock ${LOCKFLAGS} $t
REGRESS_TARGETS+=flock-$t
.endfor

TESTS_LOCKF!=	jot 2 1
.for t in ${TESTS_LOCKF}
lockf-$t: lockf
	./lockf ${LOCKFLAGS} $t
REGRESS_TARGETS+=lockf-$t
.endfor

.include <bsd.regress.mk>
