#	$Id: Makefile,v 1.1 2018/09/30 08:26:40 vgross Exp $

PROG=	cmsgsize

TESTIFACE =	vether12
TESTNET !!=	jot -r -s '.' 2 0 255
CMSG_ADDR =	10.${TESTNET}.3
CLEANFILES =	stamp-*

REGRESS_TARGETS =	run-regress \
			run-regress-cleanup

stamp-setup:
	-! ${SUDO} ifconfig ${TESTIFACE} destroy
	${SUDO} ifconfig ${TESTIFACE} inet ${CMSG_ADDR}/24 up
	${SUDO} ifconfig ${TESTIFACE}
	date >$@

run-regress: ${PROG} stamp-setup
	./${PROG} ${CMSG_ADDR}

run-regress-cleanup:
	rm -f stamp-setup
	-${SUDO} ifconfig ${TESTIFACE} destroy

.include <bsd.regress.mk>
