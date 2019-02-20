# $OpenBSD: Makefile,v 1.3 2019/02/20 22:17:14 benno Exp $

REGRESS_TARGETS	= 	network_statement md5
BGPD ?=			/usr/sbin/bgpd

network_statement:
	${SUDO} ksh -x ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

md5:
	${SUDO} ksh -x ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

.include <bsd.regress.mk>
