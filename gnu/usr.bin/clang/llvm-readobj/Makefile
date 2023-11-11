# $OpenBSD: Makefile,v 1.2 2023/11/11 18:35:40 robert Exp $

.include <bsd.own.mk>

PROG=	llvm-readobj
BINDIR=	/usr/bin
LINKS=	${BINDIR}/llvm-readobj ${BINDIR}/llvm-readelf
MAN=	llvm-readelf.1 llvm-readobj.1

DRIVER_TEMPLATE=Yes

SRCS+=	ARMWinEHPrinter.cpp \
	COFFDumper.cpp \
	COFFImportDumper.cpp \
	ELFDumper.cpp \
	llvm-readobj.cpp \
	MachODumper.cpp \
	ObjDumper.cpp \
	WasmDumper.cpp \
	Win64EHDumper.cpp \
	WindowsResourceDumper.cpp \
	XCOFFDumper.cpp

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm-readobj

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-readobj

LLVM_LIBDEPS=	LLVM

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>
