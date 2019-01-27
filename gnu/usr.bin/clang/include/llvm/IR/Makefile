# $OpenBSD: Makefile,v 1.5 2019/01/27 16:59:41 patrick Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm

DEFS=	Attributes.inc IntrinsicEnums.inc IntrinsicImpl.inc

all: ${DEFS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${DEFS}

Attributes.inc: ${LLVM_SRCS}/include/llvm/IR/Attributes.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-attrs \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicEnums.inc: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-enums \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

IntrinsicImpl.inc: ${LLVM_SRCS}/include/llvm/IR/Intrinsics.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-intrinsic-impl \
		-I${LLVM_SRCS}/include/llvm/IR -I${LLVM_SRCS}/lib/Target \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
