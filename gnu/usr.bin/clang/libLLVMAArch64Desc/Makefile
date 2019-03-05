# $OpenBSD: Makefile,v 1.4 2019/03/05 09:44:39 jsg Exp $

LIB=	LLVMAArch64Desc
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AArch64 \
		-I${LLVM_SRCS}/lib/Target/AArch64

SRCS+=	AArch64AsmBackend.cpp \
	AArch64ELFObjectWriter.cpp \
	AArch64ELFStreamer.cpp \
	AArch64MachObjectWriter.cpp \
	AArch64MCAsmInfo.cpp \
	AArch64MCCodeEmitter.cpp \
	AArch64MCExpr.cpp \
	AArch64MCTargetDesc.cpp \
	AArch64TargetStreamer.cpp \
	AArch64WinCOFFObjectWriter.cpp \
	AArch64WinCOFFStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/AArch64/MCTargetDesc
