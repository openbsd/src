#	$OpenBSD: Makefile.inc,v 1.5 2012/08/22 22:54:44 pascal Exp $

# gmon sources
.PATH: ${LIBCSRCDIR}/gmon

SRCS+=	gmon.c mcount.c
MAN+=	moncontrol.3

MLINKS+=moncontrol.3 monstartup.3

# mcount cannot be compiled with profiling or pie
mcount.po:
	@echo "${COMPILE.c} ${NOPIE_FLAGS} ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.c} ${NOPIE_FLAGS} ${.IMPSRC} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o
