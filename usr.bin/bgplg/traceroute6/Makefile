#	$OpenBSD: Makefile,v 1.1 2010/04/02 21:20:49 sthen Exp $

PROGDIR=	${.CURDIR}/../../../usr.sbin/traceroute6

LDSTATIC=	-static
CFLAGS+=	-I${PROGDIR}
NOMAN=		yes

.include "${PROGDIR}/Makefile"

BINDIR=		/var/www/bin
BINMODE=	000

.PATH:		${PROGDIR}
