#	$OpenBSD: Makefile.inc,v 1.4 2002/12/02 09:00:26 miod Exp $
#	$NetBSD: Makefile.inc,v 1.6 2002/02/24 01:04:23 matt Exp $

RELOC=0x39F000
.PATH:	${.CURDIR}/../../vax ${.CURDIR}/../common

CFLAGS+=	-fno-stack-protector
CPPFLAGS+=-I. -I${.CURDIR}/../../../../ -I${.CURDIR}/../../ -I${.CURDIR}/../common -I${.CURDIR}/../../include
CPPFLAGS+=-DRELOC=${RELOC}

# Private handling of assembler files.
.s.o:
	${CC} -x assembler-with-cpp ${CPPFLAGS} -E ${.IMPSRC} | as -o ${.TARGET}
