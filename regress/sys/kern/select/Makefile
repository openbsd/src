#	$OpenBSD: Makefile,v 1.3 2021/12/24 10:25:36 visa Exp $

PROGS=		select_close select_iocond select_regevent
LDADD=		-lpthread -lutil
WARNINGS=	yes

REGRESS_TARGETS+=	run-regress-select_close
REGRESS_TARGETS+=	run-regress-select_regevent

IOCOND_TESTS=		fifo pipe pty socket-tcp socket-udp socket-unix
.for t in ${IOCOND_TESTS}
run-regress-select_iocond-${t}: select_iocond
	./select_iocond ${t}
REGRESS_TARGETS+=	run-regress-select_iocond-${t}
.endfor
CLEANFILES+=		iocond_fifo

.include <bsd.regress.mk>
