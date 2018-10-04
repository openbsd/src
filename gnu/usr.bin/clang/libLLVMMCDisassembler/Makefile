# $OpenBSD: Makefile,v 1.3 2018/10/04 11:43:29 patrick Exp $

LIB=	LLVMMCDisassembler
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	Disassembler.cpp \
	MCDisassembler.cpp \
	MCExternalSymbolizer.cpp \
	MCRelocationInfo.cpp \
	MCSymbolizer.cpp \

.PATH:	${.CURDIR}/../../../llvm/lib/MC/MCDisassembler

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
