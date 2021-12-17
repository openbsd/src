# $OpenBSD: Makefile,v 1.6 2021/12/17 14:55:45 patrick Exp $

LIB=	LLVMDemangle
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Demangle

SRCS+=	Demangle.cpp \
	ItaniumDemangle.cpp \
	MicrosoftDemangle.cpp \
	MicrosoftDemangleNodes.cpp \
	RustDemangle.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Demangle
