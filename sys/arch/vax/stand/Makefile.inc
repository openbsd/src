#	$OpenBSD: Makefile.inc,v 1.5 2013/11/15 03:10:33 uebayasi Exp $
#	$NetBSD: Makefile.inc,v 1.6 2002/02/24 01:04:23 matt Exp $

RELOC=0x39F000
.PATH:	${.CURDIR}/../../vax ${.CURDIR}/../common

CFLAGS+=	-fno-stack-protector
CPPFLAGS+=-I. -I${.CURDIR}/../../../../ -I${.CURDIR}/../../ -I${.CURDIR}/../common -I${.CURDIR}/../../include
CPPFLAGS+=-DRELOC=${RELOC}

# Private handling of assembler files.
.s.o:
	${CC} -x assembler-with-cpp ${CPPFLAGS} -E ${.IMPSRC} | ${AS} -o ${.TARGET}
