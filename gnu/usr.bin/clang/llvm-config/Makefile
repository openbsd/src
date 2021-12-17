# $OpenBSD: Makefile,v 1.7 2021/12/17 14:55:47 patrick Exp $

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

LLVM_LIBDEPS=	LLVMTableGen LLVMSupport

reconf:
.for arch in ${NATIVE_ARCHS}
	mkdir ${.CURDIR}/CMakeGen
	cd ${.CURDIR}/CMakeGen && ${CMAKE} -DLLVM_TARGETS_TO_BUILD="${arch};AMDGPU" ${LLVMROOT}
	mv ${.CURDIR}/CMakeGen/tools/llvm-config/LibraryDependencies.inc ${.CURDIR}/LibraryDependencies.inc.${arch}
	rm -rf ${.CURDIR}/CMakeGen
.endfor

CLEANFILES+= BuildVariables.inc LibraryDependencies.inc

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-config

.include <bsd.prog.mk>

