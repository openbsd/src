#	$OpenBSD: Makefile,v 1.1.1.1 1998/06/23 18:46:42 mickey Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}
SADIR=${.CURDIR}/..

.PATH: ${S}/lib/libz

.include "${S}/lib/libz/Makefile"
CFLAGS+=${SACFLAGS} -I${S}/lib/libsa -DDYNAMIC_CRC_TABLE
