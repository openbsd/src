#	$OpenBSD: Makefile,v 1.3 2016/09/17 15:03:40 florian Exp $

PROGDIR=	${.CURDIR}/../../../sbin/ping

LDSTATIC=	${STATIC}
CFLAGS+=	-I${PROGDIR}
NOMAN=		yes

BINDIR=		/var/www/bin
LINKS=		${BINDIR}/ping ${BINDIR}/ping6

.include "${PROGDIR}/Makefile"

BINMODE=	000

.PATH:		${PROGDIR}
