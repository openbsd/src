#	$OpenBSD: Makefile,v 1.8 2012/08/30 19:29:14 pascal Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}

.PATH: ${S}/lib/libz

.include "${S}/lib/libz/Makefile"
CFLAGS+=${CEXTRAFLAGS} ${AFLAGS} -I${S} -fno-pie
