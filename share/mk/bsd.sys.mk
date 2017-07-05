#	$OpenBSD: bsd.sys.mk,v 1.12 2017/07/05 13:31:40 espie Exp $
#	$NetBSD: bsd.sys.mk,v 1.2 1995/12/13 01:25:07 cgd Exp $
#
# Overrides used for OpenBSD source tree builds.

#CFLAGS+= -Werror

.if defined(DESTDIR)
CPPFLAGS+= -nostdinc -idirafter ${DESTDIR}/usr/include
CXXFLAGS+= -idirafter ${DESTDIR}/usr/include/g++
.endif

.if defined(PARALLEL)
# Yacc
.y:
	${YACC.y} -b ${.TARGET:R} ${.IMPSRC}
	${LINK.c} -o ${.TARGET} ${.TARGET:R}.tab.c ${LDLIBS}
	rm -f ${.TARGET:R}.tab.c
.y.c:
	${YACC.y} -b ${.TARGET:R} ${.IMPSRC}
	mv ${.TARGET:R}.tab.c ${.TARGET}
.y.o:
	${YACC.y} -b ${.TARGET:R} ${.IMPSRC}
	${COMPILE.c} -o ${.TARGET} ${.TARGET:R}.tab.c
	rm -f ${.TARGET:R}.tab.c
	if test -f ${.TARGET:R}.d; then sed -i -e 's,${.TARGET:R}.tab.c,${.IMPSRC},' ${.TARGET:R}.d; fi
.endif
