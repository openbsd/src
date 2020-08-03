# $OpenBSD: Makefile,v 1.1 2020/08/03 14:45:31 patrick Exp $

PROG=	lldb-tblgen
NOMAN=

.include <bsd.own.mk>
SRCS=	LLDBOptionDefEmitter.cpp \
	LLDBPropertyDefEmitter.cpp \
	LLDBTableGen.cpp \
	LLDBTableGenUtils.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/utils/TableGen

LLVM_LIBDEPS=	LLVMTableGen LLVMSupport

install:
	@# Nothing here so far ...

.include <bsd.prog.mk>
