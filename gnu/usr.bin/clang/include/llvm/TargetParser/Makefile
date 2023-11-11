# $OpenBSD: Makefile,v 1.1 2023/11/11 18:35:36 robert Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm/llvm

HDRS=	RISCVTargetParserDef.inc

all: ${HDRS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${HDRS}

RISCVTargetParserDef.inc: ${LLVM_SRCS}/lib/Target/RISCV/RISCV.td 
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-riscv-target-def \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/RISCV \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
