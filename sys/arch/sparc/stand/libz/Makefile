#	$OpenBSD: Makefile,v 1.2 2003/12/17 04:25:40 millert Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}
SADIR=${.CURDIR}/..

.PATH: ${S}/lib/libz

.include "${S}/lib/libz/Makefile"
CFLAGS+=${SACFLAGS} -I${S}/lib/libsa ${DEBUGFLAGS}
