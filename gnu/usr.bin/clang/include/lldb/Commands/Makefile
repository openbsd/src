# $OpenBSD: Makefile,v 1.1 2020/08/03 14:45:22 patrick Exp $

.include <bsd.own.mk>

LLDB_SRCS=	${.CURDIR}/../../../../../llvm/lldb

DEFS=	CommandOptions.inc

all: ${DEFS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${DEFS}

CommandOptions.inc: ${LLDB_SRCS}/source/Commands/Options.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-option-defs \
		-I${LLDB_SRCS}/source/Commands \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
