#	$OpenBSD: Makefile,v 1.6 2013/01/01 17:17:44 miod Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}

NOPIE=

.PATH: ${S}/lib/libz

CPPFLAGS+=	${SA_CPPFLAGS}

CFLAGS+=-I${S}

.include "${S}/lib/libz/Makefile"
