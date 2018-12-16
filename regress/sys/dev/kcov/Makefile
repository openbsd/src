#	$OpenBSD: Makefile,v 1.2 2018/12/16 15:56:03 anton Exp $

PROG=		kcov
WARNINGS=	yes

TESTS+=	coverage
TESTS+=	fork
TESTS+=	exec
TESTS+=	mode
TESTS+=	open
TESTS+=	close

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
