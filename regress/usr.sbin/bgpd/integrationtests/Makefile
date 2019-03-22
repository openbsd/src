# $OpenBSD: Makefile,v 1.4 2019/03/22 20:44:31 denis Exp $

REGRESS_TARGETS	= 	network_statement md5 ovs
BGPD ?=			/usr/sbin/bgpd

network_statement:
	${SUDO} ksh -x ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

md5:
	${SUDO} ksh -x ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

ovs:
	${SUDO} ksh -x ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

.include <bsd.regress.mk>
