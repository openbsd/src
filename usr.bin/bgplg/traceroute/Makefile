#	$OpenBSD: Makefile,v 1.3 2014/07/16 20:06:27 okan Exp $

PROGDIR=	${.CURDIR}/../../../usr.sbin/traceroute

LDSTATIC=	-static
CFLAGS+=	-I${PROGDIR}
NOMAN=		yes

BINDIR=		/var/www/bin
LINKS=		${BINDIR}/traceroute ${BINDIR}/traceroute6

.include "${PROGDIR}/Makefile"

BINMODE=	000

.PATH:		${PROGDIR}
