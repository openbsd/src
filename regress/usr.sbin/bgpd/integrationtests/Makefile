# $OpenBSD: Makefile,v 1.2 2018/10/05 08:57:51 benno Exp $

REGRESS_TARGETS	= 	network_statement
BGPD ?=			/usr/sbin/bgpd

network_statement:
	${SUDO} ksh -x ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

.include <bsd.regress.mk>
