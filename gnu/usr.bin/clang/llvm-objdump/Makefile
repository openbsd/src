# $OpenBSD: Makefile,v 1.2 2021/12/17 14:55:47 patrick Exp $

.include <bsd.own.mk>

PROG=	llvm-objdump
BINDIR=	/usr/bin
LIBEXECDIR=/usr/libexec

SRCS=	llvm-objdump.cpp \
	SourcePrinter.cpp \
	COFFDump.cpp \
	ELFDump.cpp \
	MachODump.cpp \
	WasmDump.cpp \
	XCOFFDump.cpp

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm-objdump

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-objdump

LLVM_LIBDEPS=	LLVM

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>
