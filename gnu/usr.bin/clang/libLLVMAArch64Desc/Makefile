# $OpenBSD: Makefile,v 1.6 2021/04/28 12:55:38 patrick Exp $

LIB=	LLVMAArch64Desc
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AArch64 \
		-I${LLVM_SRCS}/lib/Target/AArch64

SRCS+=	AArch64AsmBackend.cpp \
	AArch64ELFObjectWriter.cpp \
	AArch64ELFStreamer.cpp \
	AArch64InstPrinter.cpp \
	AArch64MCAsmInfo.cpp \
	AArch64MCCodeEmitter.cpp \
	AArch64MCExpr.cpp \
	AArch64MCTargetDesc.cpp \
	AArch64MachObjectWriter.cpp \
	AArch64TargetStreamer.cpp \
	AArch64WinCOFFObjectWriter.cpp \
	AArch64WinCOFFStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/AArch64/MCTargetDesc
