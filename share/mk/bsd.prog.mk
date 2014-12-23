#	$OpenBSD: bsd.prog.mk,v 1.66 2014/12/23 16:35:53 deraadt Exp $
#	$NetBSD: bsd.prog.mk,v 1.55 1996/04/08 21:19:26 jtc Exp $
#	@(#)bsd.prog.mk	5.26 (Berkeley) 6/25/91

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

.include <bsd.own.mk>

.SUFFIXES: .out .o .c .cc .cpp .C .cxx .y .l .s .8 .7 .6 .5 .4 .3 .2 .1 .0

.if (defined(LDSTATIC) && !defined(STATICPIE)) || defined(NOPIE)
CFLAGS+=       ${NOPIE_FLAGS}
AFLAGS+=       ${NOPIE_FLAGS}
LDFLAGS+=      ${NOPIE_LDFLAGS}
.endif

.if ${WARNINGS:L} == "yes"
CFLAGS+=       ${CDIAGFLAGS}
CXXFLAGS+=     ${CXXDIAGFLAGS}
.endif
CFLAGS+=	${COPTS}
CXXFLAGS+=     ${CXXOPTS}

CRTBEGIN?=       ${DESTDIR}/usr/lib/crtbegin.o
CRTEND?=         ${DESTDIR}/usr/lib/crtend.o

LIBCRT0?=	${DESTDIR}/usr/lib/crt0.o
LIBC?=		${DESTDIR}/usr/lib/libc.a
LIBCRYPTO?=	${DESTDIR}/usr/lib/libcrypto.a
LIBCURSES?=	${DESTDIR}/usr/lib/libcurses.a
LIBEDIT?=	${DESTDIR}/usr/lib/libedit.a
LIBEVENT?=	${DESTDIR}/usr/lib/libevent.a
LIBEXPAT?=	${DESTDIR}/usr/lib/libexpat.a
LIBFORM?=	${DESTDIR}/usr/lib/libform.a
LIBFORMW?=	${DESTDIR}/usr/lib/libformw.a
LIBKEYNOTE?=	${DESTDIR}/usr/lib/libkeynote.a
LIBKVM?=	${DESTDIR}/usr/lib/libkvm.a
LIBL?=		${DESTDIR}/usr/lib/libl.a
LIBM?=		${DESTDIR}/usr/lib/libm.a
LIBMENU?=	${DESTDIR}/usr/lib/libmenu.a
LIBMENUW?=	${DESTDIR}/usr/lib/libmenuw.a
LIBOLDCURSES?=	${DESTDIR}/usr/lib/libocurses.a
LIBOSSAUDIO?=	${DESTDIR}/usr/lib/libossaudio.a
LIBPANEL?=	${DESTDIR}/usr/lib/libpanel.a
LIBPANELW?=	${DESTDIR}/usr/lib/libpanelw.a
LIBPCAP?=	${DESTDIR}/usr/lib/libpcap.a
LIBPERL?=	${DESTDIR}/usr/lib/libperl.a
LIBPTHREAD?=	${DESTDIR}/usr/lib/libpthread.a
LIBRPCSVC?=	${DESTDIR}/usr/lib/librpcsvc.a
LIBSKEY?=	${DESTDIR}/usr/lib/libskey.a
LIBSNDIO?=	${DESTDIR}/usr/lib/libsndio.a
LIBSSL?=	${DESTDIR}/usr/lib/libssl.a
LIBTLS?=	${DESTDIR}/usr/lib/libtls.a
LIBTERMCAP?=	${DESTDIR}/usr/lib/libtermcap.a
LIBTERMLIB?=	${DESTDIR}/usr/lib/libtermlib.a
LIBUSB?=	${DESTDIR}/usr/lib/libusbhid.a
LIBUTIL?=	${DESTDIR}/usr/lib/libutil.a
LIBY?=		${DESTDIR}/usr/lib/liby.a
LIBZ?=		${DESTDIR}/usr/lib/libz.a

.if ${MACHINE_ARCH} == "alpha" || ${MACHINE_ARCH} == "amd64" || \
    ${MACHINE_ARCH} == "arm" || ${MACHINE_ARCH} == "i386"
LIBARCH?=	${DESTDIR}/usr/lib/lib${MACHINE_ARCH}.a
.else
LIBARCH?=
.endif

.if defined(PROG)
SRCS?=	${PROG}.c
.  if !empty(SRCS:N*.h:N*.sh)
OBJS+=	${SRCS:N*.h:N*.sh:R:S/$/.o/}
_LEXINTM+=${SRCS:M*.l:.l=.c}
_YACCINTM+=${SRCS:M*.y:.y=.c}
.  endif

.  if defined(OBJS) && !empty(OBJS)
.    if !empty(SRCS:M*.C) || !empty(SRCS:M*.cc) || !empty(SRCS:M*.cpp) || \
       !empty(SRCS:M*.cxx)
${PROG}: ${LIBCRT0} ${OBJS} ${LIBC} ${CRTBEGIN} ${CRTEND} ${DPADD}
	${CXX} ${LDFLAGS} ${LDSTATIC} -o ${.TARGET} ${OBJS} ${LDADD}
.    else
${PROG}: ${LIBCRT0} ${OBJS} ${LIBC} ${CRTBEGIN} ${CRTEND} ${DPADD}
	${CC} ${LDFLAGS} ${LDSTATIC} -o ${.TARGET} ${OBJS} ${LDADD}
.    endif
.  endif	# defined(OBJS) && !empty(OBJS)

.  if	!defined(MAN)
MAN=	${PROG}.1
.  endif	# !defined(MAN)
.endif	# defined(PROG)

.MAIN: all
all: ${PROG} _SUBDIRUSE

.if !target(clean)
clean: _SUBDIRUSE
	rm -f a.out [Ee]rrs mklog *.core y.tab.h \
	    ${PROG} ${OBJS} ${_LEXINTM} ${_YACCINTM} ${CLEANFILES}
.endif

cleandir: _SUBDIRUSE clean

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif

.if !target(realinstall)
realinstall:
.if defined(PROG)
	${INSTALL} ${INSTALL_COPY} -S ${INSTALL_STRIP} \
	    -o ${BINOWN} -g ${BINGRP} \
	    -m ${BINMODE} ${PROG} ${DESTDIR}${BINDIR}/${PROG}
.endif
.endif

install: maninstall _SUBDIRUSE
.if defined(LINKS) && !empty(LINKS)
.  for lnk file in ${LINKS}
	@l=${DESTDIR}${lnk}; \
	 t=${DESTDIR}${file}; \
	 echo $$t -\> $$l; \
	 rm -f $$t; ln $$l $$t
.  endfor
.endif

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
