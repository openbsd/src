#	$OpenBSD: Makefile,v 1.2 2012/08/21 14:46:18 pascal Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}
SADIR=${.CURDIR}/..

NOPIE=

.PATH: ${S}/lib/libz

.include "${S}/lib/libz/Makefile"
CFLAGS+=${SACFLAGS} -I${S}/lib/libsa
