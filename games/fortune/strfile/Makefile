#	$OpenBSD: Makefile,v 1.2 1996/03/09 01:33:26 niklas Exp $
#	$NetBSD: Makefile,v 1.5 1995/03/23 08:28:44 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	
MAN=	strfile.8
CLEANFILES+=	strfile

all: strfile

strfile: ${.CURDIR}/strfile.c
	${HOSTCC} ${CFLAGS} ${.CURDIR}/strfile.c -o $@


.include <bsd.prog.mk>
