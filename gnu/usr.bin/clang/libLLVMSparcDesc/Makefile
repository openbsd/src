# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMSparcDesc
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Sparc \
		-I${LLVM_SRCS}/lib/Target/Sparc

.include <bsd.own.mk>
SRCS=	SparcAsmBackend.cpp \
	SparcELFObjectWriter.cpp \
	SparcMCAsmInfo.cpp \
	SparcMCCodeEmitter.cpp \
	SparcMCTargetDesc.cpp \
	SparcMCExpr.cpp \
	SparcTargetStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Sparc/MCTargetDesc

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
