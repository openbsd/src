#	$OpenBSD: Makefile,v 1.7 1997/08/04 20:25:52 mickey Exp $
#	$NetBSD: Makefile,v 1.2 1997/01/22 01:36:30 cgd Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}
SADIR=${.CURDIR}/..

.PATH: ${S}/lib/libz

all:	machine-links

machine-links:
	@rm -f machine i386
	@ln -fs ${.CURDIR}/../.. i386
	@ln -fs ${.CURDIR}/../../include machine

.include "${S}/lib/libz/Makefile"
CFLAGS+=${SACFLAGS} -I${S}/lib/libsa -DDYNAMIC_CRC_TABLE
