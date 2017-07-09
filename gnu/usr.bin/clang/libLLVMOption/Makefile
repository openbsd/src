# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMOption
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Option

.include <bsd.own.mk>
SRCS=	Arg.cpp \
	ArgList.cpp \
	Option.cpp \
	OptTable.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Option

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
