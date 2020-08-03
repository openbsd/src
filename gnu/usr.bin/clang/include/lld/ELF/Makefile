# $OpenBSD: Makefile,v 1.1 2020/08/03 14:45:22 patrick Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm/llvm
LLD_SRCS=	${.CURDIR}/../../../../../llvm/lld

DEFS=	Options.inc

all: ${DEFS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${DEFS}

Options.inc: ${LLD_SRCS}/ELF/Options.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-opt-parser-defs \
		-I ${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}


.include <bsd.obj.mk>
