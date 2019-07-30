# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:34 espie Exp $

LIB=	LLVMARMDesc
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

.include <bsd.own.mk>
SRCS=	ARMAsmBackend.cpp \
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

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
