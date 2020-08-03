# $OpenBSD: Makefile,v 1.1 2020/08/03 14:45:23 patrick Exp $

.include <bsd.own.mk>

LLDB_SRCS=	${.CURDIR}/../../../../../llvm/lldb

DEFS=	TargetProperties.inc TargetPropertiesEnum.inc

all: ${DEFS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${DEFS}

TargetProperties.inc: ${LLDB_SRCS}/source/Target/TargetProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-defs \
		-I${LLDB_SRCS}/source/Target \
		-o ${.TARGET} ${.ALLSRC}

TargetPropertiesEnum.inc: ${LLDB_SRCS}/source/Target/TargetProperties.td
	${.OBJDIR}/../../../lldb-tblgen/lldb-tblgen -gen-lldb-property-enum-defs \
		-I${LLDB_SRCS}/source/Target \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
