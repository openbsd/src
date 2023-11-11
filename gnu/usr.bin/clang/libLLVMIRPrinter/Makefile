# $OpenBSD: Makefile,v 1.1 2023/11/11 18:35:37 robert Exp $

LIB=	LLVMIRPrinter
NOPROFILE=

SRCS+=	IRPrinterIRPrintingPasses.cpp

IRPrinterIRPrintingPasses.cpp:
	ln -s ${LLVM_SRCS}/lib/IRPrinter/IRPrintingPasses.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/IRPrinter
