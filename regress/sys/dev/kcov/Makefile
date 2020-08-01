#	$OpenBSD: Makefile,v 1.11 2020/08/01 08:44:57 anton Exp $

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
TESTS+=	remote
TESTS+=	remote-close
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
