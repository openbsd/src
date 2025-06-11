# $OpenBSD: Makefile,v 1.4 2025/06/11 17:46:33 robert Exp $

.include <bsd.own.mk>

PROG=	llvm-objdump
BINDIR=	/usr/bin
LIBEXECDIR=/usr/libexec

DRIVER_TEMPLATE=Yes
DRIVER_NAME=	llvm_objdump

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
