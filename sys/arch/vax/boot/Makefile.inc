#	$OpenBSD: Makefile.inc,v 1.3 2002/06/11 09:36:23 hugh Exp $
#	$NetBSD: Makefile.inc,v 1.6 2002/02/24 01:04:23 matt Exp $

RELOC=0x39F000
.PATH:	${.CURDIR}/../../vax ${.CURDIR}/../common

CPPFLAGS+=-I. -I${.CURDIR}/../../../../ -I${.CURDIR}/../../ -I${.CURDIR}/../common -I${.CURDIR}/../../include
CPPFLAGS+=-DRELOC=${RELOC}

# Private handling of assembler files.
.s.o:
	${CC} -x assembler-with-cpp ${CPPFLAGS} -E ${.IMPSRC} | as -o ${.TARGET}
