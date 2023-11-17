# $OpenBSD: Makefile.inc,v 1.28 2023/11/17 15:46:58 visa Exp $

CLANG_SRCS?=	${.CURDIR}/../../../llvm/clang
LLDB_SRCS?=	${.CURDIR}/../../../llvm/lldb
LLD_SRCS?=	${.CURDIR}/../../../llvm/lld
LLVM_SRCS?=	${.CURDIR}/../../../llvm/llvm

DRIVER_TEMPLATE?=no

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
CXXFLAGS+=	-mxgot
.endif

CPPFLAGS+=	-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS \
		-D__STDC_FORMAT_MACROS

.if ${MACHINE_ARCH} == "arm"
TRIPLE=		armv7-unknown-openbsd${OSREV}-gnueabi
.else
TRIPLE=		${MACHINE_ARCH}-unknown-openbsd${OSREV}
.endif
.include "Makefile.arch"

# upstream defaults
CFLAGS+=	-ffunction-sections -fdata-sections
CXXFLAGS+=	-ffunction-sections -fdata-sections
CXXFLAGS+=	-std=c++17
CXXFLAGS+=	-fvisibility-inlines-hidden
CXXFLAGS+=	-fno-exceptions -fno-rtti
CXXFLAGS+=	-fno-semantic-interposition
# warnings (from upstream)
CXXFLAGS+=	-Wall -Wc++98-compat-extra-semi -Wcast-qual \
		-Wcovered-switch-default -Wctad-maybe-unsupported \
		-Wdelete-non-virtual-dtor -Werror=date-time \
		-Werror=unguarded-availability-new -Wextra \
		-Wimplicit-fallthrough -Wmisleading-indentation \
		-Wmissing-field-initializers -Wno-long-long -Wno-noexcept-type \
		-Wno-unused-parameter -Wnon-virtual-dtor -Wstring-conversion \
		-Wsuggest-override -Wwrite-strings

LDADD+=-Wl,--start-group
.for lib in ${LLVM_LIBDEPS}
DPADD+=	${.OBJDIR}/../lib${lib}/lib${lib}.a
LDADD+=	${.OBJDIR}/../lib${lib}/lib${lib}.a
.endfor
LDADD+=-Wl,--end-group

.if ${DRIVER_TEMPLATE:L} == "yes"
DRIVER_NAME?=	${PROG:S/-/_/g}
CLEANFILES+=	${DRIVER_NAME}_main.cpp
SRCS+=		${DRIVER_NAME}_main.cpp

${DRIVER_NAME}_main.cpp:
	sed "s,@TOOL_NAME@,${DRIVER_NAME},g" \
		${LLVM_SRCS}/cmake/modules/llvm-driver-template.cpp.in > $@
.endif
