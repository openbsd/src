# $OpenBSD: Makefile,v 1.2 2004/12/29 04:39:17 david Exp $

NET=	192.0.3.0
MASK=	255.255.255.0
GW=	192.0.2.1

netmask:
.for cmd in add delete
.for mod in -net -dst
	@${SUDO} route -nt ${cmd} ${mod} ${NET}/24 ${GW}
	@${SUDO} route -nt ${cmd} ${mod} ${NET} -netmask ${MASK} ${GW}
.endfor
.endfor

REGRESS_TARGETS=netmask
REGRESS_ROOT_TARGETS=${REGRESS_TARGETS}
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
