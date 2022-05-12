# $OpenBSD: Makefile.inc,v 1.26 2022/05/12 15:51:23 gkoehler Exp $

CLANG_SRCS?=	${.CURDIR}/../../../llvm/clang
LLDB_SRCS?=	${.CURDIR}/../../../llvm/lldb
LLD_SRCS?=	${.CURDIR}/../../../llvm/lld
LLVM_SRCS?=	${.CURDIR}/../../../llvm/llvm

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

CLANG_INCLUDES=	-I${CLANG_SRCS}/include
LLDB_INCLUDES=	-I${LLDB_SRCS}/include \
		-I${LLDB_SRCS}/source
CPPFLAGS+=	-I${LLVM_SRCS}/include -I${.CURDIR}/../include -I${.OBJDIR} \
		-I${.OBJDIR}/../include
CPPFLAGS+=	-DNDEBUG

# Disable some protections in the compiler to regain performance.
.if ${MACHINE_ARCH} == "aarch64" || ${MACHINE_ARCH} == "amd64" || \
    ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "mips64" || \
    ${MACHINE_ARCH} == "mips64el" || ${MACHINE_ARCH} == "powerpc"
CXXFLAGS+=	-fno-ret-protector
.endif
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
CXXFLAGS+=	-mno-retpoline
.endif

# Omit frame pointer to improve performance.
.if ${MACHINE_ARCH} == "mips64" || ${MACHINE_ARCH} == "mips64el"
CXXFLAGS+=	-fomit-frame-pointer
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

# upstream defaults
CFLAGS+=	-ffunction-sections -fdata-sections
CXXFLAGS+=	-std=c++14
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
