# $OpenBSD: Makefile,v 1.5 2026/05/29 11:18:31 robert Exp $

LIB=	lldbABI
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	ABIAArch64.cpp \
	ABIMacOSX_arm64.cpp \
	ABISysV_arm64.cpp \
	ABISysV_arc.cpp \
	ABIARM.cpp \
	ABIMacOSX_arm.cpp \
	ABISysV_arm.cpp \
	ABISysV_hexagon.cpp \
	ABIMips.cpp \
	ABISysV_loongarch.cpp \
	ABISysV_mips.cpp \
	ABISysV_mips64.cpp \
	ABIPowerPC.cpp \
	ABISysV_ppc.cpp \
	ABISysV_ppc64.cpp \
	ABISysV_s390x.cpp \
	ABIX86.cpp \
	ABIX86_i386.cpp \
	ABIMacOSX_i386.cpp \
	ABISysV_i386.cpp \
	ABISysV_x86_64.cpp \
	ABIWindows_x86_64.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ABI/AArch64
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ABI/ARC
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ABI/ARM
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ABI/Hexagon
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ABI/LoongArch
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ABI/Mips
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ABI/PowerPC
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ABI/SystemZ
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ABI/X86

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
