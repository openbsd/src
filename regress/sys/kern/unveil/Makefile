#	$OpenBSD: Makefile,v 1.7 2024/09/01 05:48:20 anton Exp $

WARNINGS=	yes

PROGS+=			syscalls
LDADD_syscalls=		-lutil
DPADD_syscalls=		${LIBUTIL}
REGRESS_ROOT_TARGETS+=	run-regress-syscalls
run-regress-syscalls:
	${SUDO} ./syscalls

PROGS+=				socket

PROGS+=access
run-regress-access: access
	./access ${.CURDIR}/access-expected

.include <bsd.regress.mk>
