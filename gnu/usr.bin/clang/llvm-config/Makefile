# $OpenBSD: Makefile,v 1.10 2025/06/11 17:46:33 robert Exp $

PROG=	llvm-config
BINDIR= /usr/bin

NATIVE_ARCHS=AArch64 ARM Mips PowerPC RISCV Sparc X86

CMAKE=		cmake
LLVMROOT=	${.CURDIR}/../../../llvm/llvm

.include <bsd.own.mk>
SRCS=	llvm-config.cpp

CPPFLAGS+= -I${.CURDIR}

llvm-config.o: BuildVariables.inc ExtensionDependencies.inc LibraryDependencies.inc

BuildVariables.inc: ${.CURDIR}/BuildVariables.inc.def
	cp ${.CURDIR}/BuildVariables.inc.def ${.OBJDIR}/BuildVariables.inc
	echo "#define LLVM_TARGETS_BUILT \"${LLVM_ARCH} AMDGPU\"" >> \
	    ${.OBJDIR}/BuildVariables.inc

ExtensionDependencies.inc:
	cp ${.CURDIR}/ExtensionDependencies.inc \
	    ${.OBJDIR}/ExtensionDependencies.inc

LibraryDependencies.inc:
	cp ${.CURDIR}/LibraryDependencies.inc.${LLVM_ARCH} \
	    ${.OBJDIR}/LibraryDependencies.inc

LLVM_LIBDEPS=	LLVM

reconf:
.for arch in ${NATIVE_ARCHS}
	mkdir -p ${.CURDIR}/CMakeGen
	${CMAKE} -B ${.CURDIR}/CMakeGen -S ${LLVMROOT} -DLLVM_TARGETS_TO_BUILD="${arch};AMDGPU" \
		-DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_BENCHMARKS=OFF \
		-DCMAKE_BUILD_TYPE=Release
	mv ${.CURDIR}/CMakeGen/tools/llvm-config/LibraryDependencies.inc ${.CURDIR}/LibraryDependencies.inc.${arch}
	rm -rf ${.CURDIR}/CMakeGen
.endfor

CLEANFILES+=	BuildVariables.inc LibraryDependencies.inc
.if ${.CURDIR} != ${.OBJDIR}
CLEANFILES+=	ExtensionDependencies.inc
.endif

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-config

.include <bsd.prog.mk>

