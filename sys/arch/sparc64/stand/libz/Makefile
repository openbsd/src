#	$OpenBSD: Makefile,v 1.4 2003/12/17 04:23:37 millert Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}
SADIR=${.CURDIR}/..

.PATH: ${S}/lib/libz

.include "${S}/lib/libz/Makefile"
CFLAGS+=${CEXTRAFLAGS} ${AFLAGS} ${SACFLAGS} -I${S}/lib/libsa ${DEBUGFLAGS}
