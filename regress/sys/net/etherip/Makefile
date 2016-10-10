#	$OpenBSD: Makefile,v 1.2 2016/10/10 18:25:26 bluhm Exp $

SYSCTL_ETHERIP !=	sysctl net.inet.etherip.allow

.if ${SYSCTL_ETHERIP:C/.*=//} != 1
regress:
	@echo "${SYSCTL_ETHERIP}"
	@echo set this sysctl to 1 for additional tests
	@echo SKIPPED
.endif

REGRESS_TARGETS=	etherip_1
REGRESS_ROOT_TARGETS=	etherip_1

RDOMAINS=		11 12
IFACE_NUMS=		11 12

etherip_1:
	${SUDO} ksh ${.CURDIR}/$@.sh -R "${RDOMAINS}" -I "${IFACE_NUMS}"

.include <bsd.regress.mk>
