# $OpenBSD: Makefile,v 1.3 2023/11/11 18:35:40 robert Exp $

.include <bsd.own.mk>

PROG=	llvm-objdump
BINDIR=	/usr/bin
LIBEXECDIR=/usr/libexec

SRCS=	llvm-objdump.cpp \
	SourcePrinter.cpp \
	COFFDump.cpp \
	ELFDump.cpp \
	MachODump.cpp \
	OffloadDump.cpp \
	WasmDump.cpp \
	XCOFFDump.cpp

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm-objdump

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-objdump

LLVM_LIBDEPS=	LLVM

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>
