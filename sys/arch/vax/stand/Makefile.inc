#	$OpenBSD: Makefile.inc,v 1.1 2000/04/27 02:34:50 bjc Exp $
#	$NetBSD: Makefile.inc,v 1.2 1999/07/18 05:55:45 abs Exp $

XXRPB=0x1F4240
RELOC=0x2f0000
.PATH:	${.CURDIR}/../../vax ${.CURDIR}/../common

CPPFLAGS+=-I${.CURDIR}/../../../../ -I${.CURDIR}/../../ -I${.CURDIR}/../common -I${.CURDIR}/../../include
CPPFLAGS+=-DXXRPB=${XXRPB} -DRELOC=${RELOC}

# Private handling of assembler files.
.s.o:
	${CC} -x assembler-with-cpp ${CPPFLAGS} -E ${.IMPSRC} | as -o ${.TARGET}
