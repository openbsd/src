# $OpenBSD: Makefile,v 1.1 2026/06/23 10:50:23 jca Exp $

LIB=	clangDependencyScanning
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${CLANG_SRCS}/lib/DependencyScanning

.include <bsd.own.mk>

SRCS=	DependencyScanningFilesystem.cpp \
	DependencyScanningService.cpp \
	DependencyScanningWorker.cpp \
	DependencyScanningUtils.cpp \
	DependencyScannerImpl.cpp \
	InProcessModuleCache.cpp \
	ModuleDepCollector.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/DependencyScanning

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
