#	$NetBSD: Makefile,v 1.1 1996/02/02 15:26:15 mrg Exp $
# from: $Id: Makefile,v 8.1 1994/12/15 06:23:45 vixie Exp 

.PATH:	${.CURDIR}/../nslookup \
	${.CURDIR}/../man

PROG=	dig
SRCS=	dig.c list.c subr.c debug.c send.c
CFLAGS+=	-I${.CURDIR}/..

.include <bsd.prog.mk>
.include "../../Makefile.inc"
