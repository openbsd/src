# $OpenBSD: Makefile,v 1.6 2019/03/05 09:44:39 jsg Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm

DEFS=	Attributes.inc IntrinsicEnums.inc IntrinsicImpl.inc

INCDIR=	/usr/include/llvm/IR

all: ${DEFS}

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

install includes: ${DEFS}
	${INSTALL} -d -m 755 ${DESTDIR}${INCDIR}
	@cd ${.OBJDIR}; for i in $(DEFS); do \
	    j="cmp -s $$i ${DESTDIR}${INCDIR}/$$i || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
		-m 444 $$i ${DESTDIR}${INCDIR}"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.obj.mk>
