# $OpenBSD: Makefile,v 1.5 2020/08/03 14:45:24 patrick Exp $

LIB=	LLVMAArch64Desc
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AArch64 \
		-I${LLVM_SRCS}/lib/Target/AArch64

SRCS+=	AArch64AsmBackend.cpp \
	AArch64ELFObjectWriter.cpp \
	AArch64ELFStreamer.cpp \
	AArch64InstPrinter.cpp \
	AArch64MachObjectWriter.cpp \
	AArch64MCAsmInfo.cpp \
	AArch64MCCodeEmitter.cpp \
	AArch64MCExpr.cpp \
	AArch64MCTargetDesc.cpp \
	AArch64TargetStreamer.cpp \
	AArch64WinCOFFObjectWriter.cpp \
	AArch64WinCOFFStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/AArch64/MCTargetDesc
