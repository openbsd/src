# $OpenBSD: Makefile,v 1.1 2019/12/23 17:32:53 denis Exp $

REGRESS_TARGETS	= 	network_statement

OSPF6D ?=			/usr/sbin/ospf6d

network_statement:
	${SUDO} ksh ${.CURDIR}/$@.sh ${OSPF6D} ${.CURDIR} 11 12 pair11 pair12

.include <bsd.regress.mk>
