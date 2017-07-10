#	$OpenBSD: Makefile,v 1.4 2017/07/10 21:30:37 espie Exp $
#	$NetBSD: Makefile,v 1.11 2003/08/01 17:04:04 lukem Exp $

.include <bsd.own.mk>

PROG=		mklocale
SRCS=		yacc.y lex.l
CPPFLAGS+=	-I. -I${.CURDIR} -I${.CURDIR}/../../lib/libc -I${.CURDIR}/../../lib/libc/include
YHEADER=	1

.include <bsd.prog.mk>
