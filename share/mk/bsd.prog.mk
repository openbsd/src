#	$OpenBSD: bsd.prog.mk,v 1.34 2002/11/10 23:03:55 mickey Exp $
#	$NetBSD: bsd.prog.mk,v 1.55 1996/04/08 21:19:26 jtc Exp $
#	@(#)bsd.prog.mk	5.26 (Berkeley) 6/25/91

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

.include <bsd.own.mk>

.SUFFIXES: .out .o .c .cc .C .cxx .y .l .s .8 .7 .6 .5 .4 .3 .2 .1 .0

.if ${WARNINGS:L} == "yes"
CFLAGS+=       ${CDIAGFLAGS}
CXXFLAGS+=     ${CXXDIAGFLAGS}
.endif
CFLAGS+=	${COPTS}
CXXFLAGS+=     ${CXXOPTS}

.if (${ELF_TOOLCHAIN} == "yes")
CRTBEGIN?=       ${DESTDIR}/usr/lib/crtbegin.o
CRTEND?=         ${DESTDIR}/usr/lib/crtend.o
.endif

LIBCRT0?=	${DESTDIR}/usr/lib/crt0.o
LIB45?=		${DESTDIR}/usr/lib/lib45.a
LIBACL?=	${DESTDIR}/usr/lib/libacl.a
LIBASN1?=	${DESTDIR}/usr/lib/libasn1.a
LIBC?=		${DESTDIR}/usr/lib/libc.a
LIBCOM_ERR?=	${DESTDIR}/usr/lib/libcom_err.a
LIBCOMPAT?=	${DESTDIR}/usr/lib/libcompat.a
LIBCRYPTO?=	${DESTDIR}/usr/lib/libcrypto.a
LIBCURSES?=	${DESTDIR}/usr/lib/libcurses.a
LIBDES?=	${DESTDIR}/usr/lib/libdes.a
LIBEDIT?=	${DESTDIR}/usr/lib/libedit.a
LIBEVENT?=	${DESTDIR}/usr/lib/libevent.a
LIBGCC?=	${DESTDIR}/usr/lib/libgcc.a
LIBHDB?=	${DESTDIR}/usr/lib/libhdb.a
LIBKADM?=	${DESTDIR}/usr/lib/libkadm.a
LIBKADM5CLNT?=	${DESTDIR}/usr/lib/libkadm5clnt.a
LIBKADM5SRV?=	${DESTDIR}/usr/lib/libkadm5srv.a
LIBKAFS?=	${DESTDIR}/usr/lib/libkafs.a
LIBKDB?=	${DESTDIR}/usr/lib/libkdb.a
LIBKEYNOTE?=	${DESTDIR}/usr/lib/libkeynote.a
LIBKRB?=	${DESTDIR}/usr/lib/libkrb.a
LIBKRB5?=	${DESTDIR}/usr/lib/libkrb5.a
LIBKVM?=	${DESTDIR}/usr/lib/libkvm.a
LIBL?=		${DESTDIR}/usr/lib/libl.a
LIBM?=		${DESTDIR}/usr/lib/libm.a
LIBOLDCURSES?=	${DESTDIR}/usr/lib/libocurses.a
LIBPCAP?=	${DESTDIR}/usr/lib/libpcap.a
LIBPERL?=	${DESTDIR}/usr/lib/libperl.a
LIBRPCSVC?=	${DESTDIR}/usr/lib/librpcsvc.a
LIBSECTOK?=	${DESTDIR}/usr/lib/libsectok.a
LIBSKEY?=	${DESTDIR}/usr/lib/libskey.a
LIBSSL?=	${DESTDIR}/usr/lib/libssl.a
LIBTELNET?=	${DESTDIR}/usr/lib/libtelnet.a
LIBTERMCAP?=	${DESTDIR}/usr/lib/libtermcap.a
LIBTERMLIB?=	${DESTDIR}/usr/lib/libtermlib.a
LIBUSB?=	${DESTDIR}/usr/lib/libusbhid.a
LIBUTIL?=	${DESTDIR}/usr/lib/libutil.a
LIBWRAP?=	${DESTDIR}/usr/lib/libwrap.a
LIBY?=		${DESTDIR}/usr/lib/liby.a
LIBZ?=		${DESTDIR}/usr/lib/libz.a

.if ${MACHINE_ARCH} == "i386"
LIBARCH?=	${DESTDIR}/usr/lib/libi386.a
.else
LIBARCH?=
.endif

# old stuff
LIBDBM?=	${DESTDIR}/usr/lib/libdbm.a
LIBMP?=		${DESTDIR}/usr/lib/libmp.a
LIBPC?=		${DESTDIR}/usr/lib/libpc.a
LIBPLOT?=	${DESTDIR}/usr/lib/libplot.a
LIBRESOLV?=	${DESTDIR}/usr/lib/libresolv.a

.if defined(SHAREDSTRINGS)
CLEANFILES+=strings
.c.o:
	${CC} -E ${CFLAGS} ${.IMPSRC} | xstr -c -
	@${CC} ${CFLAGS} -c x.c -o ${.TARGET}
	@rm -f x.c

.cc.o:
	${CXX} -E ${CXXFLAGS} ${.IMPSRC} | xstr -c -
	@mv -f x.c x.cc
	@${CXX} ${CXXFLAGS} -c x.cc -o ${.TARGET}
	@rm -f x.cc

.C.o:
	${CXX} -E ${CXXFLAGS} ${.IMPSRC} | xstr -c -
	@mv -f x.c x.C
	@${CXX} ${CXXFLAGS} -c x.C -o ${.TARGET}
	@rm -f x.C

.cxx.o:
	${CXX} -E ${CXXFLAGS} ${.IMPSRC} | xstr -c -
	@mv -f x.c x.cxx
	@${CXX} ${CXXFLAGS} -c x.cxx -o ${.TARGET}
	@rm -f x.cxx
.endif


.if defined(PROG)
SRCS?=	${PROG}.c
.if !empty(SRCS:N*.h:N*.sh)
OBJS+=	${SRCS:N*.h:N*.sh:R:S/$/.o/g}
LOBJS+=	${LSRCS:.c=.ln} ${SRCS:M*.c:.c=.ln}
.endif

.if defined(OBJS) && !empty(OBJS)
.if defined(DESTDIR)

${PROG}: ${LIBCRT0} ${OBJS} ${LIBC} ${CRTBEGIN} ${CRTEND} ${DPADD}
	${CC} ${LDFLAGS} ${LDSTATIC} -o ${.TARGET} -nostdlib -L${DESTDIR}/usr/lib ${LIBCRT0} ${CRTBEGIN} ${OBJS} ${LDADD} -lgcc -lc -lgcc ${CRTEND}

.else

${PROG}: ${LIBCRT0} ${OBJS} ${LIBC} ${CRTBEGIN} ${CRTEND} ${DPADD}
	${CC} ${LDFLAGS} ${LDSTATIC} -o ${.TARGET} ${OBJS} ${LDADD}

.endif	# defined(DESTDIR)
.endif	# defined(OBJS) && !empty(OBJS)

.if	!defined(MAN)
MAN=	${PROG}.1
.endif	# !defined(MAN)
.endif	# defined(PROG)

.MAIN: all
all: ${PROG} _SUBDIRUSE

.if !target(clean)
clean: _SUBDIRUSE
	rm -f a.out [Ee]rrs mklog core *.core \
	    ${PROG} ${OBJS} ${LOBJS} ${CLEANFILES}
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
	${INSTALL} ${INSTALL_COPY} ${INSTALL_STRIP} -o ${BINOWN} -g ${BINGRP} \
	    -m ${BINMODE} ${PROG} ${DESTDIR}${BINDIR}
.endif
.if defined(HIDEGAME)
	(cd ${DESTDIR}/usr/games; rm -f ${PROG}; ln -s dm ${PROG})
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

.if !target(lint)
lint: ${LOBJS}
.if defined(LOBJS) && !empty(LOBJS)
	@${LINT} ${LINTFLAGS} ${LDFLAGS:M-L*} ${LOBJS} ${LDADD}
.endif
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
