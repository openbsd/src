# $OpenBSD: Makefile,v 1.1 2025/06/11 17:46:32 robert Exp $

LIB=	LLVMCodeGenData
NOPROFILE=

CPPFLAGS+=-I${LLVM_SRC}/include/llvm/CodeGenData

SRCS+=	OutlinedHashTree.cpp \
	OutlinedHashTreeRecord.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/CodeGenData
