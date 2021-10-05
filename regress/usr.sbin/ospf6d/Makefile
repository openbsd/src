# $OpenBSD: Makefile,v 1.3 2021/10/05 17:42:05 anton Exp $

REGRESS_TARGETS= 	network_statement
REGRESS_ROOT_TARGETS=	${REGRESS_TARGETS}

OSPF6D ?=		/usr/sbin/ospf6d

network_statement:
	${SUDO} ksh ${.CURDIR}/$@.sh ${OSPF6D} ${.CURDIR} 11 12 pair11 pair12

.include <bsd.regress.mk>
