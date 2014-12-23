#	$OpenBSD: Makefile,v 1.4 2014/12/23 19:32:16 pascal Exp $

PROGDIR=	${.CURDIR}/../../../usr.sbin/traceroute

LDSTATIC=	${STATIC}
CFLAGS+=	-I${PROGDIR}
NOMAN=		yes

BINDIR=		/var/www/bin
LINKS=		${BINDIR}/traceroute ${BINDIR}/traceroute6

.include "${PROGDIR}/Makefile"

BINMODE=	000

.PATH:		${PROGDIR}
