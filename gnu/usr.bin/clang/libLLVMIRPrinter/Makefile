# $OpenBSD: Makefile,v 1.2 2024/02/08 20:28:54 miod Exp $

LIB=	LLVMIRPrinter
NOPROFILE=

SRCS+=	IRPrinterIRPrintingPasses.cpp

IRPrinterIRPrintingPasses.cpp:
	ln -s ${LLVM_SRCS}/lib/IRPrinter/IRPrintingPasses.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/IRPrinter

CLEANFILES+= IRPrinterIRPrintingPasses.cpp
