#	$OpenBSD: Makefile,v 1.2 1997/03/12 10:41:49 downsj Exp $
#	$NetBSD: Makefile,v 1.1 1996/02/02 15:26:21 mrg Exp $

.PATH:	${.CURDIR}/../man

CFLAGS+=${INCLUDE} ${CONFIG}

PROG=	dnsquery

LDADD=	${LIBRESOLV}

.include <bsd.prog.mk>
.include "../../Makefile.inc"
