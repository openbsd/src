#	$NetBSD: Makefile.inc,v 1.1.1.1 1995/07/25 23:12:21 chuck Exp $

.if defined(SA_PROG)

# Must have S=/usr/src/sys (or equivalent)
# But note: this is w.r.t. a subdirectory
S=	../../../..

RELOC?= 240000
DEFS?= -DSTANDALONE
INCL?= -I. -I../libsa -I${S}/lib/libsa -I${S}
COPTS?= -msoft-float ${DEFS} ${INCL}

LIBSA?= ../libsa
SRTOBJ?= ${LIBSA}/SRT0.o ${LIBSA}/SRT1.o
LIBS?= ${LIBSA}/libsa.a
MDEC_DIR?=/usr/mdec

SRCS?=	${SA_PROG}.c
OBJS?=	${SRTOBJ} ${SRCS:S/.c/.o/g}

${SA_PROG}.bin : ${SA_PROG}
	cp ${SA_PROG} a.out ; strip a.out
	dd ibs=32 skip=1 if=a.out of=$@
	-rm -f a.out

${SA_PROG} : ${OBJS} ${LIBS}
	${LD} -N -T ${RELOC} -e start -o $@ ${OBJS} ${LIBS}
	@size $@

.if !target(clean)
clean:
	-rm -f a.out [Ee]rrs mklog core *.core
	-rm -f ${SA_PROG} ${SA_PROG}.bin ${OBJS} ${CLEANFILES}
.endif

.endif
