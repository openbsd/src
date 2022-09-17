#	$OpenBSD: Makefile,v 1.5 2022/09/17 12:51:23 benno Exp $

WARNINGS=	yes

PROGS+=			syscalls
LDADD_syscalls=		-lutil
DPADD_syscalls=		${LIBUTIL}
REGRESS_ROOT_TARGETS+=	run-regress-syscalls
run-regress-syscalls:
	${SUDO} ./syscalls

PROGS+=				socket

.include <bsd.regress.mk>
