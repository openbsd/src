#	$OpenBSD: Makefile,v 1.2 2003/12/17 04:28:48 millert Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}
SADIR=${.CURDIR}/..

.PATH: ${S}/lib/libz

.include "${S}/lib/libz/Makefile"
CFLAGS+=-msoft-float ${SACFLAGS} -I${S}/lib/libsa ${DEBUGFLAGS}
