#	$OpenBSD: Makefile,v 1.9 2022/10/06 23:32:17 kn Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}

.PATH: ${S}/lib/libz

.include "${S}/lib/libz/Makefile"
CFLAGS+=${CEXTRAFLAGS} ${AFLAGS} -I${S} -fno-pie
CPPFLAGS+=-DDYNAMIC_CRC_TABLE -DBUILDFIXED
