# $OpenBSD: Makefile,v 1.17 2019/05/17 14:50:41 schwarze Exp $

.include <bsd.own.mk>

PROG=	ld.lld
BINDIR=	/usr/bin
SRCS=	lld.cpp

TARGET_ARCH?=	${MACHINE_ARCH}
.if ${LINKER_VERSION:L} == "lld"
LINKS=	${BINDIR}/ld.lld ${BINDIR}/ld

maninstall:
	cd ${DESTDIR}${MANDIR}1 && rm -f ld.1 && ln ld.lld.1 ld.1
.endif

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.CURDIR}/../../../llvm/tools/lld/include

.PATH:	${.CURDIR}/../../../llvm/tools/lld/tools/lld
.PATH:	${.CURDIR}/../../../llvm/tools/lld/docs

LLVM_LIBDEPS=	LLVM \
		lldELF \
		lldCommon \
		lldCore

.include <bsd.prog.mk>
