# $OpenBSD: Makefile,v 1.7 2002/05/11 00:20:20 espie Exp $

PROG=	pcap

DPADD+= ${LIBPCAP}
LDADD+= -lpcap

REGRESSTARGETS=		sudo-regress-${PROG}
REGRESSROOTTARGETS=	${REGRESSTARGETS}

sudo-regress-${PROG}: ${PROG}
	${SUDO} ./${PROG}

.include <bsd.regress.mk>
