# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbABI
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	ABIMacOSX_arm.cpp \
	ABIMacOSX_arm64.cpp \
	ABIMacOSX_i386.cpp \
	ABISysV_arm.cpp \
	ABISysV_arm64.cpp \
	ABISysV_hexagon.cpp \
	ABISysV_i386.cpp \
	ABISysV_mips.cpp \
	ABISysV_mips64.cpp \
	ABISysV_ppc.cpp \
	ABISysV_ppc64.cpp \
	ABISysV_s390x.cpp \
	ABISysV_x86_64.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ABI/MacOSX-arm
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ABI/MacOSX-arm64
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ABI/MacOSX-i386
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ABI/SysV-arm
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ABI/SysV-arm64
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ABI/SysV-hexagon
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ABI/SysV-i386
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ABI/SysV-mips
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ABI/SysV-mips64
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ABI/SysV-ppc
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ABI/SysV-ppc64
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ABI/SysV-s390x
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ABI/SysV-x86_64

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
