#	$OpenBSD: Makefile,v 1.2 2023/07/10 17:46:03 anton Exp $

PROGS+=	dev-limit
PROGS+=	iocmd-limit

CFLAGS+=	-Wall

REGRESS_ROOT_TARGETS=	run-regress-dev-limit \
			run-regress-iocmd-limit

TIMEOUT?=	10

#
# Create 2048 processes. Each child process will attempt
# open /dev/pf and waith for 10secs on success. If
# /dev/pf can not be opened child exits immediately.
# There should be 1023 children, which could open /dev/pf.
#
run-regress-dev-limit:
	${SUDO} ./dev-limit -c 2048 -s 1023 -t ${TIMEOUT}

#
# Open 1024 tickets for DIOCGETRULES without closing them.
# Program expects to see EBUSY and returns 0 in that case.
#
# Open and close 1024 tickets. Program closes ticket by
# DIOCXEND before issuing next DIOCGETRULES command.
# Program expects to see no error and returns 0 in tat case.
#
run-regress-iocmd-limit:
	${SUDO} ./iocmd-limit -c DIOCGETRULES -i 513
	${SUDO} ./iocmd-limit -c DIOCXEND -i 1024

.include <bsd.regress.mk>
