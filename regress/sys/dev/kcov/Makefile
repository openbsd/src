#	$OpenBSD: Makefile,v 1.4 2018/12/25 22:57:58 anton Exp $

PROG=		kcov
WARNINGS=	yes

LDADD+=	-lpthread
DPADD+=	${LIBPTHREAD}

TESTS+=	coverage
TESTS+=	fork
TESTS+=	exec
TESTS+=	mode
TESTS+=	open
TESTS+=	close
TESTS+=	dying

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
