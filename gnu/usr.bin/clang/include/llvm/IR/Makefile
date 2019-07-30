# $OpenBSD: Makefile,v 1.4 2017/07/09 15:28:34 espie Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm

DEFS=	Attributes.gen Intrinsics.gen

all: ${DEFS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${DEFS}

Attributes.gen: ${LLVM_SRCS}/include/llvm/IR/Attributes.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-attrs \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

Intrinsics.gen: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td 
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
