#	$OpenBSD: Makefile,v 1.2 2001/01/28 23:41:40 niklas Exp $
#	$NetBSD: Makefile,v 1.8 1995/03/23 08:28:36 cgd Exp $

PROG=		fortune
MAN=		fortune.6
CFLAGS+=	-I${.CURDIR}/../strfile 
LDADD+=		-lcompat
DPADD+=		${LIBCOMPAT}

.include "${.CURDIR}/../../Makefile.inc"
.include <bsd.prog.mk>
