#	$NetBSD: Makefile,v 1.8 1995/03/23 08:28:36 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=		fortune
MAN=		fortune.6
CFLAGS+=	-I${.CURDIR}/../strfile 
LDADD+=		-lcompat
DPADD+=		${LIBCOMPAT}

.include "${.CURDIR}/../../Makefile.inc"
.include <bsd.prog.mk>
