# $OpenBSD: Makefile,v 1.1 2025/06/11 17:46:32 robert Exp $

LIB=	LLVMDebugInfoBTF
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/BTF

SRCS+=	BTFContext.cpp \
	BTFParser.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/DebugInfo/BTF
