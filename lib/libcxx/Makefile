# $OpenBSD: Makefile,v 1.6 2017/04/17 15:53:21 kettenis Exp $

.include <bsd.own.mk>

.if ${COMPILER_VERSION:L} != "clang"
CC=		clang
CXX=		clang++
.endif

.if ${BUILD_CLANG:L} == "yes"

HDRDIR=		${.CURDIR}/include
SRCDIR=		${.CURDIR}/src
AHDRDIR=	${.CURDIR}/../libcxxabi/include
UHDRDIR=	${.CURDIR}/../libunwind/include
CXXINCLUDEDIR=	/usr/include/c++/v1

.PATH:	${SRCDIR}

LIB=		c++

SRCS+=		algorithm.cpp\
		any.cpp\
		bind.cpp\
		chrono.cpp\
		condition_variable.cpp\
		debug.cpp\
		exception.cpp\
		future.cpp\
		hash.cpp\
		ios.cpp\
		iostream.cpp\
		locale.cpp\
		memory.cpp\
		mutex.cpp\
		new.cpp\
		random.cpp\
		regex.cpp\
		shared_mutex.cpp\
		stdexcept.cpp\
		string.cpp\
		strstream.cpp\
		system_error.cpp\
		thread.cpp\
		typeinfo.cpp\
		utility.cpp\
		valarray.cpp

CPPFLAGS+=	-DLIBCXXABI -I${HDRDIR} \
		-DLIBCXX_BUILDING_LIBCXXABI -I${AHDRDIR} -I${UHDRDIR}
CXXFLAGS+=	-nostdlib -nostdinc++
.if empty(CXXFLAGS:M-std=*)
CXXFLAGS+=	-std=c++11
.endif

STD_HEADERS=	__bit_reference\
		__bsd_locale_defaults.h\
		__bsd_locale_fallbacks.h\
		__config\
		__debug\
		__functional_03\
		__functional_base\
		__functional_base_03\
		__hash_table\
		__locale\
		__mutex_base\
		__nullptr\
		__refstring\
		__split_buffer\
		__sso_allocator\
		__std_stream\
		__threading_support\
		__tree\
		__tuple\
		__undef___deallocate\
		__undef_min_max\
		algorithm\
		array\
		atomic\
		bitset\
		cassert\
		ccomplex\
		cctype\
		cerrno\
		cfenv\
		cfloat\
		chrono\
		cinttypes\
		ciso646\
		climits\
		clocale\
		cmath\
		codecvt\
		complex\
		complex.h\
		condition_variable\
		csetjmp\
		csignal\
		cstdarg\
		cstdbool\
		cstddef\
		cstdint\
		cstdio\
		cstdlib\
		cstring\
		ctgmath\
		ctime\
		ctype.h\
		cwchar\
		cwctype\
		deque\
		errno.h\
		exception\
		float.h\
		forward_list\
		fstream\
		functional\
		future\
		initializer_list\
		inttypes.h\
		iomanip\
		ios\
		iosfwd\
		iostream\
		istream\
		iterator\
		limits\
		list\
		locale\
		map\
		math.h\
		memory\
		mutex\
		new\
		numeric\
		ostream\
		queue\
		random\
		ratio\
		regex\
		scoped_allocator\
		set\
		setjmp.h\
		shared_mutex\
		sstream\
		stack\
		stdbool.h\
		stddef.h\
		stdexcept\
		stdio.h\
		stdlib.h\
		streambuf\
		string\
		string.h\
		strstream\
		system_error\
		tgmath.h\
		thread\
		tuple\
		type_traits\
		typeindex\
		typeinfo\
		unordered_map\
		unordered_set\
		utility\
		valarray\
		vector\
		wchar.h\
		wctype.h

.for hdr in ${STD_HEADERS}
STD+=		${hdr} ${HDRDIR}/${hdr}
.endfor
STDDIR=		${CXXINCLUDEDIR}

EXT_HEADERS=	__hash\
		hash_map\
		hash_set

.for hdr in ${EXT_HEADERS}
EXT+=		${hdr} ${HDRDIR}/ext/${hdr}
.endfor
EXTDIR=		${CXXINCLUDEDIR}/ext

EXP_HEADERS=	__config\
		__memory\
		algorithm\
		any\
		chrono\
		deque\
		dynarray\
		filesystem\
		forward_list\
		functional\
		iterator\
		list\
		map\
		memory_resource\
		optional\
		propagate_const\
		ratio\
		regex\
		set\
		string\
		string_view\
		system_error\
		tuple\
		type_traits\
		unordered_map\
		unordered_set\
		utility\
		vector

.for hdr in ${EXP_HEADERS}
EXP+=		${hdr} ${HDRDIR}/experimental/${hdr}
.endfor
EXPDIR=		${CXXINCLUDEDIR}/experimental

XLOCALE_HEADERS=	__nop_locale_mgmt.h\
			__posix_l_fallback.h\
			__strtonum_fallback.h\
			xlocale.h

.for hdr in ${XLOCALE_HEADERS}
XLOCALE+=	${hdr} ${HDRDIR}/support/xlocale/${hdr}
.endfor
XLOCALEDIR=	${CXXINCLUDEDIR}/support/xlocale

includes:
	@echo installing ${STD_HEADERS}
	${INSTALL} -d -m 755 ${DESTDIR}/${STDDIR};
.	for hdr src in ${STD}
		cmp -s ${src} ${DESTDIR}/${STDDIR}/${hdr} || \
		    ${INSTALL} ${INSTALL_COPY} -m 444 ${src} ${DESTDIR}/${STDDIR}/${hdr};
.	endfor
	${INSTALL} -d -m 755 ${DESTDIR}/${EXTDIR};
.	for hdr src in ${EXT}
		cmp -s ${src} ${DESTDIR}/${EXTDIR}/${hdr} || \
		    ${INSTALL} ${INSTALL_COPY} -m 444 ${src} ${DESTDIR}/${EXTDIR}/${hdr};
.	endfor
	${INSTALL} -d -m 755 ${DESTDIR}/${EXPDIR};
.	for hdr src in ${EXP}
		cmp -s ${src} ${DESTDIR}/${EXPDIR}/${hdr} || \
		    ${INSTALL} ${INSTALL_COPY} -m 444 ${src} ${DESTDIR}/${EXPDIR}/${hdr};
.	endfor
	${INSTALL} -d -m 755 ${DESTDIR}/${CXXINCLUDEDIR}/support/newlib;
	cmp -s ${HDRDIR}/support/newlib/xlocale.h \
	    ${DESTDIR}/${CXXINCLUDEDIR}/support/newlib/xlocale.h || \
	    ${INSTALL} ${INSTALL_COPY} -m 444 ${HDRDIR}/support/newlib/xlocale.h \
	    ${DESTDIR}/${CXXINCLUDEDIR}/support/newlib/xlocale.h;
	${INSTALL} -d -m 755 ${DESTDIR}/${CXXINCLUDEDIR}/support/xlocale;
.	for hdr src in ${XLOCALE}
		cmp -s ${src} ${DESTDIR}/${XLOCALEDIR}/${hdr} || \
		    ${INSTALL} ${INSTALL_COPY} -m 444 ${src} ${DESTDIR}/${XLOCALEDIR}/${hdr};
.	endfor

.include <bsd.lib.mk>

.else
NOPROG=
.include <bsd.prog.mk>
.endif
