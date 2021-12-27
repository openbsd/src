#	$OpenBSD: Makefile,v 1.4 2021/12/27 16:20:20 visa Exp $

PROGS=		select_close select_iocond select_regevent
LDADD=		-lpthread -lutil
WARNINGS=	yes

REGRESS_TARGETS+=	run-regress-select_close
REGRESS_TARGETS+=	run-regress-select_regevent

IOCOND_TESTS=		fifo pipe pty socket-tcp socket-udp socket-unix
.for t in ${IOCOND_TESTS}
run-regress-select_iocond-${t}: select_iocond
	./select_iocond ${t}
# Disabled for now, some of the tests are unreliable.
#REGRESS_TARGETS+=	run-regress-select_iocond-${t}
.endfor
CLEANFILES+=		iocond_fifo

.include <bsd.regress.mk>
