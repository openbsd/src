# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:41 jsg Exp $

LIB=	LLVMOption
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Option

SRCS+=	Arg.cpp \
	ArgList.cpp \
	Option.cpp \
	OptTable.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Option
