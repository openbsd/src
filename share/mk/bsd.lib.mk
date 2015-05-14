#	$OpenBSD: bsd.lib.mk,v 1.72 2015/05/14 02:56:01 jsg Exp $
#	$NetBSD: bsd.lib.mk,v 1.67 1996/01/17 20:39:26 mycroft Exp $
#	@(#)bsd.lib.mk	5.26 (Berkeley) 5/2/91

.include <bsd.own.mk>				# for 'NOPIC' definition

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

.if exists(${.CURDIR}/shlib_version)
.include "${.CURDIR}/shlib_version"
.if defined(LIB) && defined(LIB${LIB}_VERSION)
SHLIB_MAJOR=${LIB${LIB}_VERSION:R}
SHLIB_MINOR=${LIB${LIB}_VERSION:E}
.else
SHLIB_MAJOR=${major}
SHLIB_MINOR=${minor}
.endif
.endif

.MAIN: all

# prefer .S to a .c, add .po, remove stuff not used in the BSD libraries.
# .so used for PIC object files.
# .do used for distrib "crunchgen" object files
# .m for objective c files.
.SUFFIXES:
.SUFFIXES: .out .o .go .po .so .do .S .s .c .cc .cpp .C .cxx .f .y .l .m4 .m

.if defined(NOPIE)
CFLAGS+=	${NOPIE_FLAGS}
CXXFLAGS+=	${NOPIE_FLAGS}
AFLAGS+=	${NOPIE_FLAGS}
.endif

DIST_CFLAGS+=	-Os

.c.o:
	@echo "${COMPILE.c} ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.c} ${.IMPSRC}  -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.c.go:
	@echo "${COMPILE.c} -g ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.c} -g ${.IMPSRC} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.c.po:
	@echo "${COMPILE.c} -p ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.c} -p ${.IMPSRC} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.c.so:
	@echo "${COMPILE.c} ${PICFLAG} -DPIC ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.c} ${PICFLAG} -DPIC ${.IMPSRC} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.c.do:
	@echo "${COMPILE.c} ${DIST_CFLAGS} ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.c} ${DIST_CFLAGS} ${.IMPSRC}  -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.cc.o .cpp.o .C.o .cxx.o:
	@echo "${COMPILE.cc} ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.cc} ${.IMPSRC} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.cc.go .cpp.go .C.go .cxx.go:
	@echo "${COMPILE.cc} -g ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.cc} -g ${.IMPSRC} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.cc.po .cpp.po .C.po .cxx.po:
	@echo "${COMPILE.cc} -p ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.cc} -p ${.IMPSRC} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.cc.so .cpp.so .C.so .cxx.so:
	@echo "${COMPILE.cc} ${PICFLAG} -DPIC ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.cc} ${PICFLAG} -DPIC ${.IMPSRC} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

# Fortran 77
.f.o:
	@echo "${COMPILE.f} ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.f} ${.IMPSRC} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.f.go:
	@echo "${COMPILE.f} -g ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.f} -g ${.IMPSRC} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.f.po:
	@echo "${COMPILE.f} -p ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.f} -p ${.IMPSRC} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.f.so:
	@echo "${COMPILE.f} ${PICFLAG} -DPIC ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.f} ${PICFLAG} -DPIC ${.IMPSRC} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.S.o .s.o:
	@echo "${COMPILE.S} ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.S} ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.S.go .s.go:
	@echo "${COMPILE.S} ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.S} ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.S.po .s.po:
	@echo "${COMPILE.S} -DPROF ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} \
	    -o ${.TARGET}"
	@${COMPILE.S} -DPROF ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} \
	    -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.S.so .s.so:
	@echo "${COMPILE.S} ${PICFLAG} ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} \
	    -o ${.TARGET}"
	@${COMPILE.S} ${PICFLAG} ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} \
	    -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.S.do .s.do:
	@echo "${COMPILE.S} ${CFLAGS:M-[ID]*} ${AINC} ${DIST_CFLAGS} \
	    ${.IMPSRC} -o ${.TARGET}"
	@${COMPILE.S} ${CFLAGS:M-[ID]*} ${AINC} ${DIST_CFLAGS} ${.IMPSRC} \
	    -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.if ${WARNINGS:L} == "yes"
CFLAGS+=	${CDIAGFLAGS}
CXXFLAGS+=	${CXXDIAGFLAGS}
.endif
CFLAGS+=	${COPTS}
CXXFLAGS+=	${CXXOPTS}

DEBUG?=	-g

_LIBS=lib${LIB}.a
.if (${DEBUGLIBS:L} == "yes")
_LIBS+=lib${LIB}_g.a
.endif
.if !defined(NOPROFILE)
_LIBS+=lib${LIB}_p.a
.endif

.if !defined(NOPIC)
.if defined(SHLIB_MAJOR) && defined(SHLIB_MINOR)
_LIBS+=lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
.endif
.endif

all: ${_LIBS} _SUBDIRUSE

OBJS+=	${SRCS:N*.h:R:S/$/.o/}

lib${LIB}.a: ${OBJS}
	@echo building standard ${LIB} library
	@rm -f lib${LIB}.a
	@${AR} cq lib${LIB}.a `${LORDER} ${OBJS} | tsort -q`
	${RANLIB} lib${LIB}.a

GOBJS+=	${OBJS:.o=.go}
lib${LIB}_g.a: ${GOBJS}
	@echo building debugging ${LIB} library
	@rm -f lib${LIB}_g.a
	@${AR} cq lib${LIB}_g.a `${LORDER} ${GOBJS} | tsort -q`
	${RANLIB} lib${LIB}_g.a

POBJS+=	${OBJS:.o=.po}
lib${LIB}_p.a: ${POBJS}
	@echo building profiled ${LIB} library
	@rm -f lib${LIB}_p.a
	@${AR} cq lib${LIB}_p.a `${LORDER} ${POBJS} | tsort -q`
	${RANLIB} lib${LIB}_p.a

SOBJS+=	${OBJS:.o=.so}
lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR}: ${SOBJS} ${DPADD}
	@echo building shared ${LIB} library \(version ${SHLIB_MAJOR}.${SHLIB_MINOR}\)
	@rm -f lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
	${CC} -shared ${PICFLAG} \
	    -o lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
	    `${LORDER} ${SOBJS}|tsort -q` ${LDADD}

# all .do files...
DOBJS+=	${OBJS:.o=.do}

# .do files that we actually need for where this dist lib will be used
.if defined(DIST_OBJS)
SELECTED_DOBJS=${DIST_OBJS:.o=.do}
.else
SELECTED_DOBJS?=${DOBJS}
.endif

DIST_LIB?=lib${LIB}_d.a
${DIST_LIB}: ${SELECTED_DOBJS}
	@echo building distrib ${DIST_LIB} library from ${SELECTED_DOBJS}
	@rm -f ${DIST_LIB}
.if !empty(SELECTED_DOBJS)
	@${AR} cq ${DIST_LIB} `${LORDER} ${SELECTED_DOBJS} | tsort -q`
.else
	@${AR} cq ${DIST_LIB}
.endif
	${RANLIB} ${DIST_LIB}

.if !target(clean)
clean: _SUBDIRUSE
	rm -f a.out [Ee]rrs mklog *.core ${CLEANFILES}
	rm -f lib${LIB}.a ${OBJS}
	rm -f lib${LIB}_g.a ${GOBJS}
	rm -f lib${LIB}_p.a ${POBJS}
	rm -f lib${LIB}.so.*.* ${SOBJS}
	rm -f ${DIST_LIB} ${DOBJS}
.endif

cleandir: _SUBDIRUSE clean

.if defined(SRCS)
afterdepend: .depend
	@(TMP=`mktemp -q /tmp/_dependXXXXXXXXXX`; \
	if [ $$? -ne 0 ]; then \
		echo "$$0: cannot create temp file, exiting..."; \
		exit 1; \
	fi; \
	sed -e 's/^\([^\.]*\).o[ ]*:/\1.o \1.go \1.po \1.so \1.do:/' \
	      < .depend > $$TMP; \
	mv $$TMP .depend)
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif

realinstall:
#	ranlib lib${LIB}.a
	${INSTALL} ${INSTALL_COPY} -S -o ${LIBOWN} -g ${LIBGRP} -m 600 lib${LIB}.a \
	    ${DESTDIR}${LIBDIR}/lib${LIB}.a
.if (${INSTALL_COPY} != "-p")
	${RANLIB} -t ${DESTDIR}${LIBDIR}/lib${LIB}.a
.endif
	chmod ${LIBMODE} ${DESTDIR}${LIBDIR}/lib${LIB}.a
.if (${DEBUGLIBS:L} == "yes")
#	ranlib lib${LIB}_g.a
	${INSTALL} ${INSTALL_COPY} -S -o ${LIBOWN} -g ${LIBGRP} -m 600 \
	    lib${LIB}_g.a ${DESTDIR}${LIBDIR}/debug/lib${LIB}.a
.if (${INSTALL_COPY} != "-p")
	${RANLIB} -t ${DESTDIR}${LIBDIR}/debug/lib${LIB}.a
.endif
	chmod ${LIBMODE} ${DESTDIR}${LIBDIR}/debug/lib${LIB}.a
.endif
.if !defined(NOPROFILE)
#	ranlib lib${LIB}_p.a
	${INSTALL} ${INSTALL_COPY} -S -o ${LIBOWN} -g ${LIBGRP} -m 600 \
	    lib${LIB}_p.a ${DESTDIR}${LIBDIR}
.if (${INSTALL_COPY} != "-p")
	${RANLIB} -t ${DESTDIR}${LIBDIR}/lib${LIB}_p.a
.endif
	chmod ${LIBMODE} ${DESTDIR}${LIBDIR}/lib${LIB}_p.a
.endif
.if !defined(NOPIC) && defined(SHLIB_MAJOR) && defined(SHLIB_MINOR)
	${INSTALL} ${INSTALL_COPY} -S -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR} ${DESTDIR}${LIBDIR}
.endif
.if defined(LINKS) && !empty(LINKS)
.  for lnk file in ${LINKS}
	@l=${DESTDIR}${lnk}; \
	 t=${DESTDIR}${file}; \
	 echo $$t -\> $$l; \
	 rm -f $$t; ln $$l $$t
.  endfor
.endif

install: maninstall _SUBDIRUSE
maninstall: afterinstall
afterinstall: realinstall
realinstall: beforeinstall
.endif

.if !defined(NOMAN)
.include <bsd.man.mk>
.endif

.if !defined(NONLS)
.include <bsd.nls.mk>
.endif

.include <bsd.obj.mk>
.include <bsd.dep.mk>
.include <bsd.subdir.mk>
.include <bsd.sys.mk>
