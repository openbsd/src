# $OpenBSD: Makefile,v 1.1 2021/04/28 12:55:38 patrick Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../../llvm/llvm

DEFS=	ACC.h.inc \
	ACC.cpp.inc \
	ACC.cpp

all: ${DEFS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${DEFS}

ACC.h.inc: ${LLVM_SRCS}/include/llvm/Frontend/OpenACC/ACC.td
	${.OBJDIR}/../../../../llvm-tblgen/llvm-tblgen -gen-directive-decl \
		-I${LLVM_SRCS}/include/llvm/Frontend/OpenACC \
		-I${LLVM_SRCS}/include/llvm/Frontend/ \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

ACC.cpp.inc: ${LLVM_SRCS}/include/llvm/Frontend/OpenACC/ACC.td
	${.OBJDIR}/../../../../llvm-tblgen/llvm-tblgen -gen-directive-gen \
		-I${LLVM_SRCS}/include/llvm/Frontend/OpenACC \
		-I${LLVM_SRCS}/include/llvm/Frontend/ \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

ACC.cpp: ${LLVM_SRCS}/include/llvm/Frontend/OpenACC/ACC.td
	${.OBJDIR}/../../../../llvm-tblgen/llvm-tblgen -gen-directive-impl \
		-I${LLVM_SRCS}/include/llvm/Frontend/OpenACC \
		-I${LLVM_SRCS}/include/llvm/Frontend/ \
		-I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
