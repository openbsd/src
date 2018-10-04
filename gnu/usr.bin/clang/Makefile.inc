# $OpenBSD: Makefile.inc,v 1.8 2018/10/04 11:43:29 patrick Exp $

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
.if (${MACHINE} == "amd64" || ${MACHINE} == "arm64")
CPPFLAGS+=	-fno-ret-protector
.endif
CPPFLAGS+=	-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS \
		-D__STDC_FORMAT_MACROS

.if ${MACHINE_ARCH} == "arm"
TRIPLE=		armv7-unknown-openbsd${OSREV}-gnueabi
.else
TRIPLE=		${MACHINE_ARCH}-unknown-openbsd${OSREV}
.endif
CPPFLAGS+=	-DLLVM_DEFAULT_TARGET_TRIPLE=\"${TRIPLE}\"
CPPFLAGS+=	-DLLVM_HOST_TRIPLE=\"${TRIPLE}\"
CPPFLAGS+=	-DLLVM_PREFIX=\"/usr\"
.include "Makefile.arch"
CPPFLAGS+=-DLLVM_NATIVE_ARCH=\"${LLVM_ARCH}\"
CPPFLAGS+=-DLLVM_NATIVE_ASMPARSER=LLVMInitialize${LLVM_ARCH}AsmParser
CPPFLAGS+=-DLLVM_NATIVE_ASMPRINTER=LLVMInitialize${LLVM_ARCH}AsmPrinter
CPPFLAGS+=-DLLVM_NATIVE_DISASSEMBLER=LLVMInitialize${LLVM_ARCH}Disassembler
CPPFLAGS+=-DLLVM_NATIVE_TARGET=LLVMInitialize${LLVM_ARCH}Target
CPPFLAGS+=-DLLVM_NATIVE_TARGETINFO=LLVMInitialize${LLVM_ARCH}TargetInfo
CPPFLAGS+=-DLLVM_NATIVE_TARGETMC=LLVMInitialize${LLVM_ARCH}TargetMC
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
