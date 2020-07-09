# $OpenBSD: Makefile,v 1.2 2020/06/12 13:27:43 denis Exp $

REGRESS_TARGETS	= 	network_statement

OSPF6D ?=		/usr/sbin/ospf6d

network_statement:
	${SUDO} ksh ${.CURDIR}/$@.sh ${OSPF6D} ${.CURDIR} 11 12 pair11 pair12

.include <bsd.regress.mk>
