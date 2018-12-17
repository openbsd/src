#	$OpenBSD: Makefile,v 1.3 2018/12/17 22:11:37 anton Exp $

PROG=		kcov
WARNINGS=	yes

TESTS+=	coverage
TESTS+=	fork
TESTS+=	exec
TESTS+=	mode
TESTS+=	open
TESTS+=	close

REGRESS_SETUP_ONCE=	setup
setup: ${PROG}

.for t in ${TESTS}
$t:
	@echo "\n======== ${@} ========"
	if ${SUDO} ./${PROG} -p; then \
		${SUDO} ./${PROG} ${KCOVFLAGS} $t; \
	else \
		echo "SKIPPED"; \
	fi
REGRESS_TARGETS+=	$t
.endfor

.include <bsd.regress.mk>
