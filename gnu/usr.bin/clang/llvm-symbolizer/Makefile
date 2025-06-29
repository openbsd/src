# $OpenBSD: Makefile,v 1.1 2025/06/29 01:36:32 jmatthew Exp $

.include <bsd.own.mk>

PROG=	llvm-symbolizer
BINDIR=	/usr/bin
LINKS=  ${BINDIR}/llvm-symbolizer ${BINDIR}/llvm-addr2line
MAN=    llvm-symbolizer.1 llvm-addr2line.1


DRIVER_TEMPLATE=yes

SRCS=	llvm-symbolizer.cpp

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm-symbolizer

.PATH:	${.CURDIR}/../../../llvm/llvm/tools/llvm-symbolizer

LLVM_LIBDEPS=	LLVM

LDADD+= -L ${.OBJDIR}/../libLLVM -lLLVM

.include <bsd.prog.mk>
