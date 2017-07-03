#	$OpenBSD: bsd.dep.mk,v 1.18 2017/07/03 15:11:02 espie Exp $
#	$NetBSD: bsd.dep.mk,v 1.12 1995/09/27 01:15:09 christos Exp $

.if !target(depend)
depend:
	@:
.endif

# relies on DEPS defined by bsd.lib.mk and bsd.prog.mk
.if defined(DEPS) && !empty(DEPS)
.  for o in ${DEPS}
     sinclude $o
.  endfor
.endif

CFLAGS += -MD -MP
CXXFLAGS += -MD -MP

# libraries need some special love
DFLAGS = -MD -MP -MT $*.o -MT $*.po -MT $*.so -MT $*.do

.if !target(tags)
.  if defined(SRCS)
tags: ${SRCS} _SUBDIRUSE
	-cd ${.CURDIR}; ${CTAGS} -f /dev/stdout -d ${.ALLSRC:N*.h} | \
	    sed "s;\${.CURDIR}/;;" > tags
.  else
tags:
.  endif
.endif

# explicitly tag most source files
.for i in ${SRCS:N*.[hyl]:N*.sh} ${_LEXINTM} ${_YACCINTM}
${i:R:S/$/.o/}: $i
.endfor

CLEANFILES += ${DEPS} .depend
BUILDFIRST ?=
BUILDAFTER ?=
.if !empty(BUILDFIRST) && !empty(BUILDAFTER)
${BUILDAFTER}: ${BUILDFIRST}
.endif
