#	$OpenBSD: Makefile,v 1.3 1997/03/12 10:41:50 downsj Exp $
#	$NetBSD: Makefile,v 1.2 1996/05/14 13:55:47 mrg Exp $

.PATH: ${.CURDIR}/../man

PROG = host
SRCS = host.c send.c vers.c

CFLAGS+=${INCLUDE} ${CONFIG}

LDADD=	${LIBRESOLV}

.include "../../Makefile.inc"
.include <bsd.prog.mk>
