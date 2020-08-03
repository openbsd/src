# $OpenBSD: Makefile,v 1.1 2020/08/03 14:45:23 patrick Exp $

.include <bsd.own.mk>

LLDB_SRCS=	${.CURDIR}/../../../../../llvm/lldb

DEFS=	InterpreterProperties.inc InterpreterPropertiesEnum.inc

all: ${DEFS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${DEFS}

InterpreterProperties.inc: ${LLDB_SRCS}/source/Interpreter/InterpreterProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-defs \
		-I${LLDB_SRCS}/source/Interpreter \
		-o ${.TARGET} ${.ALLSRC}

InterpreterPropertiesEnum.inc: ${LLDB_SRCS}/source/Interpreter/InterpreterProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-enum-defs \
		-I${LLDB_SRCS}/source/Interpreter \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
