# $OpenBSD: Makefile,v 1.5 2019/06/22 08:26:26 claudio Exp $

REGRESS_TARGETS	= 	network_statement md5 ovs mrt
BGPD ?=			/usr/sbin/bgpd

CLEANFILES +=		*.mrt *.out

network_statement:
	${SUDO} ksh -x ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

md5:
	${SUDO} ksh -x ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

ovs:
	${SUDO} ksh -x ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

mrt:
	${SUDO} ksh -x ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11

.include <bsd.regress.mk>
