# $OpenBSD: Makefile,v 1.7 2023/11/11 18:35:37 robert Exp $

LIB=	LLVMDemangle
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Demangle

SRCS+=	Demangle.cpp \
	ItaniumDemangle.cpp \
	MicrosoftDemangle.cpp \
	MicrosoftDemangleNodes.cpp \
	RustDemangle.cpp \
	DLangDemangle.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Demangle
