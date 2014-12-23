#	$OpenBSD: Makefile,v 1.2 2014/12/23 19:32:16 pascal Exp $

PROGDIR=	${.CURDIR}/../../../sbin/ping

LDSTATIC=	${STATIC}
CFLAGS+=	-I${PROGDIR}
NOMAN=		yes

.include "${PROGDIR}/Makefile"

BINDIR=		/var/www/bin
BINMODE=	000

.PATH:		${PROGDIR}
