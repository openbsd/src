#	$OpenBSD: Makefile,v 1.2 2014/12/23 19:32:16 pascal Exp $

PROGDIR=	${.CURDIR}/../../../usr.sbin/bgpctl

LDSTATIC=	${STATIC}
CFLAGS+=	-I${PROGDIR} -I${PROGDIR}/../bgpd
NOMAN=		yes

.include "${PROGDIR}/Makefile"

BINDIR=		/var/www/bin
BINMODE=	000

.PATH:		${PROGDIR} ${PROGDIR}/../bgpd
