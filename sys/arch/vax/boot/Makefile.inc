#	$OpenBSD: Makefile.inc,v 1.2 2000/10/04 04:03:10 bjc Exp $
#	$NetBSD: Makefile.inc,v 1.2 1999/07/18 05:55:45 abs Exp $

XXRPB=0x354240
RELOC=0x39F000
.PATH:	${.CURDIR}/../../vax ${.CURDIR}/../common

CPPFLAGS+=-I${.CURDIR}/../../../../ -I${.CURDIR}/../../ -I${.CURDIR}/../common -I${.CURDIR}/../../include
CPPFLAGS+=-DXXRPB=${XXRPB} -DRELOC=${RELOC}

# Private handling of assembler files.
.s.o:
	${CC} -x assembler-with-cpp ${CPPFLAGS} -E ${.IMPSRC} | as -o ${.TARGET}
