# $OpenBSD: Makefile,v 1.4 2020/08/09 15:56:40 patrick Exp $

PROG=	llvm-config
BINDIR= /usr/bin

NATIVE_ARCHS=AArch64 ARM Mips PowerPC Sparc X86

PYTHON=		python2
LLVMBUILD=	${.CURDIR}/../../../llvm/llvm/utils/llvm-build/llvm-build

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
	${PYTHON} -B ${LLVMBUILD} \
	    --native-target ${LLVM_ARCH} \
	    --enable-targets "${arch} AMDGPU" \
	    --write-library-table ${.CURDIR}/LibraryDependencies.inc.${arch}
.endfor

CLEANFILES+= BuildVariables.inc LibraryDependencies.inc

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-config

.include <bsd.prog.mk>

