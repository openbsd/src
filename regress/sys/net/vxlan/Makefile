#	$OpenBSD: Makefile,v 1.5 2022/04/29 17:27:38 bluhm Exp $

REGRESS_TARGETS=	vxlan_1 vxlan_2.inet vxlan_2.dyn
REGRESS_ROOT_TARGETS=	vxlan_1 vxlan_2.inet vxlan_2.dyn

RDOMAINS=		11 12
IFACE_NUMS=		11 12

vxlan_1:
	${SUDO} ksh ${.CURDIR}/$@.sh -R "${RDOMAINS}" -I "${IFACE_NUMS}"

vxlan_2.inet:
	${SUDO} ksh ${.CURDIR}/${@:R}.sh -R "40 41 42 43 44" -f inet

vxlan_2.dyn:
	env SUDO="${SUDO}" DYNAMIC=1 \
	    ksh ${.CURDIR}/${@:R}.sh -R "40 41 42 43 44" -f inet

vxlan_2.inet6:
	${SUDO} ksh ${.CURDIR}/${@:R}.sh -R "40 41 42 43 44" -f inet6

vxlan_2.dyn6:
	env SUDO="${SUDO}" DYNAMIC=1 \
	    ksh ${.CURDIR}/${@:R}.sh -R "40 41 42 43 44" -f inet6

.include <bsd.regress.mk>
