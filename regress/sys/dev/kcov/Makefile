#	$OpenBSD: Makefile,v 1.10 2019/05/14 20:48:45 anton Exp $

PROG=		kcov
WARNINGS=	yes

LDADD+=	-lpthread
DPADD+=	${LIBPTHREAD}

MODES+=	pc
MODES+=	cmp

BUFSIZES+=	1
BUFSIZES+=	2

TESTS+=	close
TESTS+=	coverage
TESTS+=	dying
TESTS+=	exec
TESTS+=	fork
TESTS+=	open
TESTS+=	state

REGRESS_SETUP_ONCE=	setup
setup: ${PROG}

.for m in ${MODES}
.  for b in ${BUFSIZES}
.    for t in ${TESTS}
$m-$b-$t:
	@echo "\n======== ${@} ========"
	if ${SUDO} ./${PROG} -p; then \
		${SUDO} ./${PROG} ${KCOVFLAGS} -b $b -m $m $t; \
	else \
		echo "SKIPPED"; \
	fi
REGRESS_TARGETS+=	$m-$b-$t
.    endfor
.  endfor
.endfor

.include <bsd.regress.mk>
