#	$OpenBSD: Makefile.inc,v 1.6 2016/03/30 06:38:41 jmc Exp $

# gmon sources
.PATH: ${LIBCSRCDIR}/gmon

SRCS+=	gmon.c mcount.c
MAN+=	moncontrol.3

# mcount cannot be compiled with profiling or pie
mcount.po:
	@echo "${COMPILE.c} ${NOPIE_FLAGS} ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.c} ${NOPIE_FLAGS} ${.IMPSRC} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o
