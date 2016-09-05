# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

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

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
