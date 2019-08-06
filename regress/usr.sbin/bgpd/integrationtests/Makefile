# $OpenBSD: Makefile,v 1.6 2019/08/06 07:31:53 claudio Exp $

REGRESS_TARGETS	= 	network_statement md5 ovs mrt
BGPD ?=			/usr/sbin/bgpd

CLEANFILES +=		*.mrt *.out

network_statement:
	${SUDO} ksh ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

md5:
	${SUDO} ksh ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

ovs:
	${SUDO} ksh ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

mrt:
	${SUDO} ksh ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11

.include <bsd.regress.mk>
