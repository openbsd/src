# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:39 jsg Exp $

LIB=	LLVMARMDesc
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

SRCS+=	ARMAsmBackend.cpp \
	ARMELFObjectWriter.cpp \
	ARMELFStreamer.cpp \
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

.PATH:	${.CURDIR}/../../../llvm/lib/Target/ARM/MCTargetDesc
