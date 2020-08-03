# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:27 patrick Exp $

LIB=	LLVMSparcDesc
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Sparc \
		-I${LLVM_SRCS}/lib/Target/Sparc

SRCS+=	SparcAsmBackend.cpp \
	SparcELFObjectWriter.cpp \
	SparcInstPrinter.cpp \
	SparcMCAsmInfo.cpp \
	SparcMCCodeEmitter.cpp \
	SparcMCTargetDesc.cpp \
	SparcMCExpr.cpp \
	SparcTargetStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/Sparc/MCTargetDesc
