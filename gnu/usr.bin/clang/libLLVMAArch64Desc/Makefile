# $OpenBSD: Makefile,v 1.3 2017/10/04 20:57:09 patrick Exp $

LIB=	LLVMAArch64Desc
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AArch64 \
		-I${LLVM_SRCS}/lib/Target/AArch64

.include <bsd.own.mk>
SRCS=	AArch64AsmBackend.cpp \
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

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
