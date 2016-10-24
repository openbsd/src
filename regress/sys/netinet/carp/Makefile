#	$OpenBSD: Makefile,v 1.1 2016/10/24 02:52:02 yasuoka Exp $

REGRESS_TARGETS=	carp_1 carp_2 carp_3 carp_4
REGRESS_ROOT_TARGETS=	carp_1 carp_2 carp_3 carp_4

RDOMAINS=		11 12
IFACE_NUMS=		11 12

#TESTOPTS=		-v

carp_1 carp_2 carp_3 carp_4:
	${SUDO} ksh ${.CURDIR}/$@.sh -R "${RDOMAINS}" -I "${IFACE_NUMS}" \
	    ${TESTOPTS}

.include <bsd.regress.mk>
