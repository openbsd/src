#	$OpenBSD: Makefile,v 1.10 1999/02/14 19:38:45 mickey Exp $
#	$NetBSD: Makefile,v 1.2 1997/01/22 01:36:30 cgd Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}
SADIR=${.CURDIR}/..

.PATH: ${S}/lib/libz

.include "${S}/lib/libz/Makefile"
CFLAGS+=${SACFLAGS} -I${S}/lib/libsa -DDYNAMIC_CRC_TABLE ${DEBUGFLAGS}
