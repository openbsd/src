# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

LIB=	LLVMMCDisassembler
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	Disassembler.cpp \
	MCRelocationInfo.cpp \
	MCExternalSymbolizer.cpp \
	MCDisassembler.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/MC/MCDisassembler

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
