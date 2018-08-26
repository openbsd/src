#	$OpenBSD: Makefile,v 1.1 2018/08/26 08:12:09 anton Exp $

PROG=		kcov
WARNINGS=	yes

run-regress-${PROG}: ${PROG}
	if ${SUDO} ./${PROG} -p; then \
		${SUDO} ./${PROG} ${KCOVFLAGS}; \
	else \
		echo "SKIPPED"; \
	fi

.include <bsd.regress.mk>
