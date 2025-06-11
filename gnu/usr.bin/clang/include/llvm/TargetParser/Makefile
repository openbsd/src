# $OpenBSD: Makefile,v 1.2 2025/06/11 17:46:32 robert Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm/llvm

HDRS=	ARMTargetParserDef.inc \
	AArch64TargetParserDef.inc \
	RISCVTargetParserDef.inc

all: ${HDRS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${HDRS}

ARMTargetParserDef.inc: ${LLVM_SRCS}/lib/Target/ARM/ARM.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-arm-target-def \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/ARM \
		-o ${.TARGET} ${.ALLSRC}

AArch64TargetParserDef.inc: ${LLVM_SRCS}/lib/Target/AArch64/AArch64.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-arm-target-def \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AArch64 \
		-o ${.TARGET} ${.ALLSRC}

RISCVTargetParserDef.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td 
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-riscv-target-def \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/RISCV \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
