#	$OpenBSD: Makefile,v 1.6 2003/12/17 05:17:07 millert Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}

.PATH: ${S}/lib/libz

.include "${S}/lib/libz/Makefile"
CFLAGS+=${CEXTRAFLAGS} ${AFLAGS} -I${S}
