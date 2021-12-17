# $OpenBSD: Makefile,v 1.2 2021/12/17 14:55:44 patrick Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../../llvm/llvm

DEFS=	OMP.h.inc \
	OMP.inc

all: ${DEFS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${DEFS}

OMP.h.inc: ${LLVM_SRCS}/include/llvm/Frontend/OpenMP/OMP.td
	${.OBJDIR}/../../../../llvm-tblgen/llvm-tblgen -gen-directive-decl \
		-I${LLVM_SRCS}/include/llvm/Frontend/OpenMP \
		-I${LLVM_SRCS}/include/llvm/Frontend/ \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

OMP.inc: ${LLVM_SRCS}/include/llvm/Frontend/OpenMP/OMP.td
	${.OBJDIR}/../../../../llvm-tblgen/llvm-tblgen -gen-directive-impl \
		-I${LLVM_SRCS}/include/llvm/Frontend/OpenMP \
		-I${LLVM_SRCS}/include/llvm/Frontend/ \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
