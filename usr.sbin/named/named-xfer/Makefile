#	$OpenBSD: Makefile,v 1.2 1997/03/12 10:42:43 downsj Exp $
#	$NetBSD: Makefile,v 1.2 1996/02/04 12:02:12 mrg Exp $
# from: $Id: Makefile,v 8.2 1995/12/31 23:28:01 vixie Exp 

.PATH:	${.CURDIR}/../named \
	${.CURDIR}/../man

PROG=	named-xfer
SRCS=	named-xfer.c db_glue.c storage.c version.c
CFLAGS+=	${INCLUDE} -I${.CURDIR}/../named ${CONFIG}
LDADD=	${LIBRESOLV}
MAN=	named-xfer.8
BINDIR=	/usr/libexec

.include <bsd.prog.mk>
.include "../../Makefile.inc"
