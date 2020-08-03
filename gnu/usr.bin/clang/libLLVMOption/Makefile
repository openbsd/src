# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:26 patrick Exp $

LIB=	LLVMOption
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Option

SRCS+=	Arg.cpp \
	ArgList.cpp \
	Option.cpp \
	OptTable.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Option
