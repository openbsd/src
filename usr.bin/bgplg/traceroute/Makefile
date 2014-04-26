#	$OpenBSD: Makefile,v 1.2 2014/04/26 11:02:45 florian Exp $

PROGDIR=	${.CURDIR}/../../../usr.sbin/traceroute

LDSTATIC=	-static
CFLAGS+=	-I${PROGDIR}
NOMAN=		yes

BINDIR=		/var/www/bin
LINKS=		${BINDIR}/traceroute ${BINDIR}/traceroute6

.include "${PROGDIR}/Makefile"

BINMODE=	000

.PATH:		${PROGDIR}

