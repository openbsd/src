#	$OpenBSD: Makefile,v 1.5 2018/12/27 10:10:13 anton Exp $

PROG=		kcov
WARNINGS=	yes

LDADD+=	-lpthread
DPADD+=	${LIBPTHREAD}

TESTS+=	close
TESTS+=	coverage
TESTS+=	dying
TESTS+=	exec
TESTS+=	fork
TESTS+=	open
TESTS+=	state

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
