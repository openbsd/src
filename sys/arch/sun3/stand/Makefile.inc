#	$NetBSD: Makefile.inc,v 1.5 1995/10/13 21:44:11 gwr Exp $

.if defined(SA_PROG)

# Must have S=/usr/src/sys (or equivalent)
# But note: this is w.r.t. a subdirectory
S=	${.CURDIR}/../../../..

RELOC?= 240000
DEFS?= -DSTANDALONE
INCL?= -I${.CURDIR} -I${.CURDIR}/../libsa -I${S}/lib/libsa -I${S}
COPTS?= -msoft-float ${DEFS} ${INCL}

LIBSA?= ../libsa
SRTOBJ?= ${LIBSA}/SRT0.o ${LIBSA}/SRT1.o
LIBS?= ${LIBSA}/libsa.a
MDEC_DIR?=/usr/mdec

SRCS?=	${SA_PROG}.c
OBJS?=	${SRCS:S/.c/.o/g}

CLEANFILES+= ${SA_PROG} ${SA_PROG}.bin

# Make a copy of the executable with its exec header removed,
# and with its length padded to a multiple of 1k bytes.
# (The padding is for convenience when making tapes.)
${SA_PROG}.bin : ${SA_PROG}
	cp ${SA_PROG} a.out ; strip a.out
	dd if=a.out ibs=32 skip=1 of=$@ obs=1k conv=osync
	-rm -f a.out

${SA_PROG} : ${SRTOBJ} ${OBJS} ${LIBS}
	${LD} -N -T ${RELOC} -e start -o $@ ${SRTOBJ} ${OBJS} ${LIBS}
	@size $@

.if !target(clean)
clean:
	-rm -f a.out [Ee]rrs mklog core *.core
	-rm -f ${CLEANFILES} *.o
.endif

.endif
