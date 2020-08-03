# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:24 patrick Exp $

LIB=	LLVMARMDesc
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

SRCS+=	ARMAsmBackend.cpp \
	ARMELFObjectWriter.cpp \
	ARMELFStreamer.cpp \
	ARMInstPrinter.cpp \
	ARMMachObjectWriter.cpp \
	ARMMachORelocationInfo.cpp \
	ARMMCAsmInfo.cpp \
	ARMMCCodeEmitter.cpp \
	ARMMCExpr.cpp \
	ARMMCTargetDesc.cpp \
	ARMTargetStreamer.cpp \
	ARMUnwindOpAsm.cpp \
	ARMWinCOFFObjectWriter.cpp \
	ARMWinCOFFStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/ARM/MCTargetDesc
