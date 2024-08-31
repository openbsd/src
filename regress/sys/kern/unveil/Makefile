#	$OpenBSD: Makefile,v 1.6 2024/08/31 20:03:03 jeremy Exp $

WARNINGS=	yes

PROGS+=			syscalls
LDADD_syscalls=		-lutil
DPADD_syscalls=		${LIBUTIL}
REGRESS_ROOT_TARGETS+=	run-regress-syscalls
run-regress-syscalls:
	${SUDO} ./syscalls

PROGS+=				socket
PROGS+=				access

.include <bsd.regress.mk>
