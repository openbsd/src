# $OpenBSD: Makefile.inc,v 1.18 2019/11/26 10:58:23 sthen Exp $

LLVM_SRCS?=	${.CURDIR}/../../../llvm

.if ${COMPILER_VERSION:L} != "clang"
CC=		clang
CXX=		clang++
.endif

BOOTSTRAP_CLANG?=no
.if ${BOOTSTRAP_CLANG} == "yes"
CC=		egcc
CXX=		eg++
.endif

DEBUG=
NOPIE=

CLANG_INCLUDES=	-I${LLVM_SRCS}/tools/clang/include
LLDB_INCLUDES=	-I${LLVM_SRCS}/tools/lldb/include \
		-I${LLVM_SRCS}/tools/lldb/source
CPPFLAGS+=	-I${LLVM_SRCS}/include -I${.CURDIR}/../include -I${.OBJDIR} \
		-I${.OBJDIR}/../include
CPPFLAGS+=	-DNDEBUG

# Disable some protections in the compiler to regain performance.
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "mips64" || ${MACHINE_ARCH} == "mips64el" || \
    ${MACHINE_ARCH} == "aarch64" 
CXXFLAGS+=	-fno-ret-protector
.endif
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
CXXFLAGS+=	-mno-retpoline
.endif

# Omit frame pointer to improve performance.
.if ${MACHINE_ARCH} == "mips64" || ${MACHINE_ARCH} == "mips64el"
CXXFLAGS+=	-fomit-frame-pointer
.endif

# Compiling with -fno-pie doesn't work on powerpc.
.if ${MACHINE_ARCH} == "powerpc"
NOPIE_FLAGS=	-fPIE
.endif

CPPFLAGS+=	-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS \
		-D__STDC_FORMAT_MACROS

.if ${MACHINE_ARCH} == "arm"
TRIPLE=		armv7-unknown-openbsd${OSREV}-gnueabi
.else
TRIPLE=		${MACHINE_ARCH}-unknown-openbsd${OSREV}
.endif
CPPFLAGS+=	-DLLVM_PREFIX=\"/usr\"
.include "Makefile.arch"
CPPFLAGS+=-DLLDB_DISABLE_PYTHON

# upstream defaults
CFLAGS+=	-ffunction-sections
.if ${MACHINE_ARCH} != "powerpc"
# XXX
CFLAGS+=        -fdata-sections
.endif
CXXFLAGS+=	-std=c++11
CXXFLAGS+=	-fvisibility-inlines-hidden
CXXFLAGS+=	-fno-exceptions -fno-rtti
# warnings (from upstream)
CXXFLAGS+=	-Wall -W -Wno-unused-parameter -Wwrite-strings -Wcast-qual \
		-Wno-missing-field-initializers -pedantic -Wno-long-long \
		-Wdelete-non-virtual-dtor -Wno-comment

LDADD+=-Wl,--start-group
.for lib in ${LLVM_LIBDEPS}
DPADD+=	${.OBJDIR}/../lib${lib}/lib${lib}.a
LDADD+=	${.OBJDIR}/../lib${lib}/lib${lib}.a
.endfor
LDADD+=-Wl,--end-group

.if ${MACHINE_ARCH} == "powerpc"
LDFLAGS+=-Wl,-relax
.endif
