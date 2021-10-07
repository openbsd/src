# $OpenBSD: Makefile,v 1.4 2021/10/07 15:20:35 anton Exp $

REGRESS_TARGETS= 	network_statement
REGRESS_ROOT_TARGETS=	${REGRESS_TARGETS}

OSPF6D ?=		/usr/sbin/ospf6d

network_statement:
	${SUDO} ksh ${.CURDIR}/$@.sh ${OSPF6D} ${.CURDIR} ${.OBJDIR} 11 12 pair11 pair12

.include <bsd.regress.mk>
