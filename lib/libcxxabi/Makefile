# $OpenBSD: Makefile,v 1.5 2017/01/20 07:55:20 patrick Exp $

.include <bsd.own.mk>

.if ${COMPILER_VERSION:L} == "clang"

HDRDIR=		${.CURDIR}/include
SRCDIR=		${.CURDIR}/src
SHDRDIR=	${.CURDIR}/../libcxx/include
UHDRDIR=	${.CURDIR}/../libunwind/include
CXXINCLUDEDIR=	/usr/include/c++/v1

.PATH: ${SRCDIR} ${.CURDIR}/../libunwind/src

CC=		clang
CXX=		clang++

LIB=		c++abi

SRCS+=		abort_message.cpp\
		cxa_aux_runtime.cpp\
		cxa_default_handlers.cpp\
		cxa_demangle.cpp\
		cxa_exception.cpp\
		cxa_exception_storage.cpp\
		cxa_guard.cpp\
		cxa_handlers.cpp\
		cxa_new_delete.cpp\
		cxa_personality.cpp\
		cxa_thread_atexit.cpp\
		cxa_unexpected.cpp\
		cxa_vector.cpp\
		cxa_virtual.cpp\
		exception.cpp\
		private_typeinfo.cpp\
		stdexcept.cpp\
		typeinfo.cpp\
		Unwind-EHABI.cpp\
		Unwind-sjlj.c\
		UnwindLevel1-gcc-ext.c\
		UnwindLevel1.c\
		UnwindRegistersRestore.S\
		UnwindRegistersSave.S\
		libunwind.cpp

CPPFLAGS+=	-I${SHDRDIR} -I${HDRDIR} -I${UHDRDIR}
CPPFLAGS+=	-D_LIBUNWIND_IS_NATIVE_ONLY
CXXFLAGS+=	-nostdlib -nostdinc++ -funwind-tables
.if empty(CXXFLAGS:M-std=*)
CXXFLAGS+=	-std=c++11
.endif

STD_HEADERS=	cxxabi.h \
		__cxxabi_config.h

.for hdr in ${STD_HEADERS}
STD+=		${hdr} ${HDRDIR}/${hdr}
.endfor
STDDIR=		${CXXINCLUDEDIR}

STD_UHEADERS=	libunwind.h \
		__libunwind_config.h \
		unwind.h

.for hdr in ${STD_UHEADERS}
USTD+=		${hdr} ${UHDRDIR}/${hdr}
.endfor
USTDDIR=	${CXXINCLUDEDIR}

MACHO_HEADERS=compact_unwind_encoding.h

.for hdr in ${MACHO_HEADERS}
MACHO+=		${hdr} ${UHDRDIR}/mach-o/${hdr}
.endfor
MACHODIR=	${CXXINCLUDEDIR}/mach-o

includes:
	@echo installing ${STD_HEADERS}
	${INSTALL} -d -m 755 ${DESTDIR}/${STDDIR};
.	for hdr src in ${STD}
		cmp -s ${src} ${DESTDIR}/${STDDIR}/${hdr} || \
		${INSTALL} ${INSTALL_COPY} -m 444 ${src} ${DESTDIR}/${STDDIR}/${hdr};
.	endfor
	${INSTALL} -d -m 755 ${DESTDIR}/${USTDDIR};
.	for hdr src in ${USTD}
		cmp -s ${src} ${DESTDIR}/${USTDDIR}/${hdr} || \
		${INSTALL} ${INSTALL_COPY} -m 444 ${src} ${DESTDIR}/${USTDDIR}/${hdr};
.	endfor
	${INSTALL} -d -m 755 ${DESTDIR}/${MACHODIR};
.	for hdr src in ${MACHO}
		cmp -s ${src} ${DESTDIR}/${MACHODIR}/${hdr} || \
		${INSTALL} ${INSTALL_COPY} -m 444 ${src} ${DESTDIR}/${MACHODIR}/${hdr};
.	endfor

.include <bsd.lib.mk>

.else
NOPROG=
.include <bsd.prog.mk>
.endif
