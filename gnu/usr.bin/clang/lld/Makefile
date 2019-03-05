# $OpenBSD: Makefile,v 1.15 2019/03/05 09:44:42 jsg Exp $

.include <bsd.own.mk>

PROG=	ld.lld
BINDIR=	/usr/bin
SRCS=	lld.cpp

TARGET_ARCH?=	${MACHINE_ARCH}
.if ${LINKER_VERSION:L} == "lld"
LINKS=	${BINDIR}/ld.lld ${BINDIR}/ld
MLINKS=	ld.lld.1 ld.1
.endif

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.CURDIR}/../../../llvm/tools/lld/include

.PATH:	${.CURDIR}/../../../llvm/tools/lld/tools/lld
.PATH:	${.CURDIR}/../../../llvm/tools/lld/docs

LLVM_LIBDEPS=	lldELF \
		lldCommon \
		lldCore

DPADD+= ${.OBJDIR}/../libLLVM/libLLVM.a
LDADD+= ${.OBJDIR}/../libLLVM/libLLVM.a

.include <bsd.prog.mk>
