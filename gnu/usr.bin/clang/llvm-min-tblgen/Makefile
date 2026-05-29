# $OpenBSD: Makefile,v 1.2 2026/05/29 11:18:32 robert Exp $

PROG=	llvm-min-tblgen
NOMAN=

.include <bsd.own.mk>

# TableGen
SRCS=	ARMTargetDefEmitter.cpp \
	Attributes.cpp \
	CodeGenIntrinsics.cpp \
	DirectiveEmitter.cpp \
	IntrinsicEmitter.cpp \
	RISCVTargetDefEmitter.cpp \
	RuntimeLibcallsEmitter.cpp \
	RuntimeLibcalls.cpp \
	SDNodeProperties.cpp \
	TableGen.cpp \
	TargetFeaturesEmitter.cpp \
	TargetLibraryInfoEmitter.cpp \
	VTEmitter.cpp \
	llvm-min-tblgen.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/utils/TableGen \
	${.CURDIR}/../../../llvm/llvm/utils/TableGen/Basic

LLVM_LIBDEPS=	LLVMTableGen LLVMSupport

install:
	@# Nothing here so far ...

.include <bsd.prog.mk>
