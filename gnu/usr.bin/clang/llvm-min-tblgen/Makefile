# $OpenBSD: Makefile,v 1.1 2025/06/11 17:46:33 robert Exp $

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
	SDNodeProperties.cpp \
	TableGen.cpp \
	VTEmitter.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/utils/TableGen \
	${.CURDIR}/../../../llvm/llvm/utils/TableGen/Basic

LLVM_LIBDEPS=	LLVMTableGen LLVMSupport

install:
	@# Nothing here so far ...

.include <bsd.prog.mk>
