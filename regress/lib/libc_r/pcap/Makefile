# $OpenBSD: Makefile,v 1.6 2002/02/17 01:12:09 marc Exp $

PROG=	pcap

LDADD+= -lpcap

REGRESSTARGETS=		sudo-regress-${PROG}
REGRESSROOTTARGETS=	${REGRESSTARGETS}

sudo-regress-${PROG}: ${PROG}
	${SUDO} ./${PROG}

.include <bsd.regress.mk>
