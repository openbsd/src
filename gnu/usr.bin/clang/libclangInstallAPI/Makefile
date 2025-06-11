# $OpenBSD: Makefile,v 1.1 2025/06/11 17:46:33 robert Exp $

LIB=	clangInstallAPI
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>

SRCS=	DiagnosticBuilderWrappers.cpp \
	DirectoryScanner.cpp \
	DylibVerifier.cpp \
	FileList.cpp \
	Frontend.cpp \
	HeaderFile.cpp \
	Library.cpp \
	Visitor.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/InstallAPI

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
