#	$OpenBSD: Makefile,v 1.4 2002/05/11 00:20:20 espie Exp $
#	$NetBSD: Makefile,v 1.2 1996/05/14 13:55:47 mrg Exp $

.PATH: ${.CURDIR}/../man

PROG = host
SRCS = host.c send.c vers.c

CFLAGS+=${INCLUDE} ${CONFIG}

.include "../../Makefile.inc"
.include <bsd.prog.mk>
