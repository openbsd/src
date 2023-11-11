# $OpenBSD: Makefile,v 1.1 2023/11/11 18:35:35 robert Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../../llvm/llvm

DEFS=	COFFOptions.inc

all: ${DEFS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${DEFS}

COFFOptions.inc: ${LLVM_SRCS}/lib/ExecutionEngine/JITLink/COFFOptions.td
	${.OBJDIR}/../../../../llvm-tblgen/llvm-tblgen -gen-opt-parser-defs \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
