# $OpenBSD: Makefile,v 1.1 2021/11/23 10:30:08 robert Exp $

LIB=	LLVMLibDriver
NOPROFILE=

SRCS+=	LibDriver.cpp

LibDriver.o: Options.inc
Options.inc: Options.td
	${.OBJDIR}/../llvm-tblgen/llvm-tblgen -gen-opt-parser-defs \
		-I ${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ToolDrivers/llvm-lib

CLEANFILES+=	Options.inc

install:
	@# Nothing here so far ...

.include <bsd.lib.mk> 
