#	$OpenBSD: Makefile,v 1.1 2016/10/07 02:06:57 yasuoka Exp $

REGRESS_TARGETS=	etherip_1
REGRESS_ROOT_TARGETS=	etherip_1

RDOMAINS=		11 12
IFACE_NUMS=		11 12

etherip_1:
	${SUDO} ksh ${.CURDIR}/$@.sh -R "${RDOMAINS}" -I "${IFACE_NUMS}"

.include <bsd.regress.mk>
