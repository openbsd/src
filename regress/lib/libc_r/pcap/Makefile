# $OpenBSD: Makefile,v 1.8 2002/09/02 20:01:43 avsm Exp $

PROG=	pcap

DPADD+= ${LIBPCAP}
LDADD+= -lpcap

REGRESS_TARGETS=		sudo-regress-${PROG}
REGRESS_ROOT_TARGETS=	${REGRESS_TARGETS}

sudo-regress-${PROG}: ${PROG}
	${SUDO} ./${PROG}

.include <bsd.regress.mk>
