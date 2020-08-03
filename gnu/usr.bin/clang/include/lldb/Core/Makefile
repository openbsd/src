# $OpenBSD: Makefile,v 1.1 2020/08/03 14:45:22 patrick Exp $

.include <bsd.own.mk>

LLDB_SRCS=	${.CURDIR}/../../../../../llvm/lldb

DEFS=	CoreProperties.inc CorePropertiesEnum.inc

all: ${DEFS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${DEFS}

CoreProperties.inc: ${LLDB_SRCS}/source/Core/CoreProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-defs \
		-I${LLDB_SRCS}/source/Core \
		-o ${.TARGET} ${.ALLSRC}

CorePropertiesEnum.inc: ${LLDB_SRCS}/source/Core/CoreProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-enum-defs \
		-I${LLDB_SRCS}/source/Core \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
