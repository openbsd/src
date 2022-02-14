#	$OpenBSD: Makefile,v 1.10 2022/02/14 06:39:19 visa Exp $

PROGS=		poll_close poll_iocond poll_regevent pollnval pollretval
LDADD=		-lpthread -lutil
WARNINGS=	yes

REGRESS_TARGETS+=	run-regress-poll_close
REGRESS_TARGETS+=	run-regress-poll_regevent
REGRESS_TARGETS+=	run-regress-pollnval
REGRESS_TARGETS+=	run-regress-pollretval

IOCOND_TESTS=		fifo pipe pty socket-tcp socket-udp socket-unix
.for t in ${IOCOND_TESTS}
run-regress-poll_iocond-${t}: poll_iocond
	./poll_iocond ${t}
# Disabled for now, some of the tests are unreliable.
#REGRESS_TARGETS+=	run-regress-poll_iocond-${t}
.endfor
CLEANFILES+=		iocond_fifo

.include <bsd.regress.mk>
