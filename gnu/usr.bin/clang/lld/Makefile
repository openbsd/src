# $OpenBSD: Makefile,v 1.18 2020/08/03 14:45:31 patrick Exp $

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
CPPFLAGS+=	-I${.CURDIR}/../../../llvm/lld/include

.PATH:	${.CURDIR}/../../../llvm/lld/tools/lld
.PATH:	${.CURDIR}/../../../llvm/lld/docs

LLVM_LIBDEPS=	LLVM \
		lldELF \
		lldCommon \
		lldCore

.include <bsd.prog.mk>
