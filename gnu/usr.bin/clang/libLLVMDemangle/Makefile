# $OpenBSD: Makefile,v 1.5 2020/08/03 14:45:25 patrick Exp $

LIB=	LLVMDemangle
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Demangle

SRCS+=	Demangle.cpp \
	ItaniumDemangle.cpp \
	MicrosoftDemangle.cpp \
	MicrosoftDemangleNodes.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Demangle
