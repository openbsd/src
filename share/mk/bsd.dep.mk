#	$OpenBSD: bsd.dep.mk,v 1.19 2017/07/04 00:59:11 espie Exp $
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
# assume libraries
${i:R:S/$/.o/} ${i:R:S/$/.po/} ${i:R:S/$/.so/} ${i:R:S/$/.do/}: $i
.endfor

CLEANFILES += ${DEPS} .depend

BUILDFIRST ?=
BUILDAFTER ?=
.if !empty(BUILDAFTER)
.  for i in ${BUILDFIRST} ${_LEXINTM} ${_YACCINTM}
.    if !exists($i)
${BUILDAFTER}: $i
.    endif
.  endfor
.endif
