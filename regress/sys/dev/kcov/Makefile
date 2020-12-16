#	$OpenBSD: Makefile,v 1.13 2020/12/16 22:59:54 bluhm Exp $

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
TESTS+=	remote-interrupt
TESTS+=	state

REGRESS_SETUP=		${PROG}

.for m in ${MODES}
.  for b in ${BUFSIZES}
.    for t in ${TESTS}
run-$m-$b-$t:
	if ${SUDO} ./${PROG} -p; then \
		${SUDO} ./${PROG} ${KCOVFLAGS} -b $b -m $m $t; \
	else \
		echo "SKIPPED"; \
	fi
REGRESS_TARGETS+=	run-$m-$b-$t
.    endfor
.  endfor
.endfor

.include <bsd.regress.mk>
