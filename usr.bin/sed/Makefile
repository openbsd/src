#	$OpenBSD: Makefile,v 1.3 2003/08/27 07:54:38 jmc Exp $

PROG=	sed
SRCS=	compile.c main.c misc.c process.c

# These get installed verbatim
.if make(install)
SUBDIR+= USD.doc
.endif

.include <bsd.prog.mk>
