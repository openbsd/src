# $OpenBSD: Makefile,v 1.2 2016/09/08 18:05:30 pascal Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm

DEFS=	Attributes.inc Intrinsics.gen

all: ${DEFS}

install:
	# Nothing here so far ...

depend:
	# Nothing here so far ...

clean cleandir:
	rm -f ${DEFS}

Attributes.inc: ${LLVM_SRCS}/include/llvm/IR/Attributes.td
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
