# $OpenBSD: Makefile,v 1.1 2004/09/24 12:54:13 markus Exp $

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
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
