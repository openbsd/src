#	$OpenBSD: Makefile,v 1.4 2022/09/05 05:34:25 anton Exp $

WARNINGS=	yes

PROGS+=			syscalls
LDADD_syscalls=		-lutil
DPADD_syscalls=		${LIBUTIL}
REGRESS_ROOT_TARGETS+=	run-regress-syscalls
run-regress-syscalls:
	${SUDO} ./syscalls

PROGS+=				socket
REGRESS_EXPECTED_FAILURES+=	run-regress-socket

.include <bsd.regress.mk>
