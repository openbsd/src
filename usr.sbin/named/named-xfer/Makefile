#	$OpenBSD: Makefile,v 1.4 2002/05/11 00:20:20 espie Exp $
#	$NetBSD: Makefile,v 1.2 1996/02/04 12:02:12 mrg Exp $
# from: $Id: Makefile,v 8.2 1995/12/31 23:28:01 vixie Exp 

.PATH:	${.CURDIR}/../named \
	${.CURDIR}/../man

PROG=	named-xfer
SRCS=	named-xfer.c db_glue.c storage.c version.c
CFLAGS+= ${INCLUDE} -I${.CURDIR}/../named ${CONFIG}
MAN=	named-xfer.8
BINDIR=	/var/named
LDSTATIC= ${STATIC}

.include <bsd.prog.mk>
.include "../../Makefile.inc"
