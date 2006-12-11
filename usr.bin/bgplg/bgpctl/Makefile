#	$OpenBSD: Makefile,v 1.1 2006/12/11 23:10:10 reyk Exp $

PROGDIR=	${.CURDIR}/../../../usr.sbin/bgpctl

LDSTATIC=	-static
CFLAGS+=	-I${PROGDIR} -I${PROGDIR}/../bgpd
NOMAN=		yes

.include "${PROGDIR}/Makefile"

BINDIR=		/var/www/bin
BINMODE=	000

.PATH:		${PROGDIR} ${PROGDIR}/../bgpd
