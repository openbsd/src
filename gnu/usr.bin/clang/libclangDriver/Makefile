# $OpenBSD: Makefile,v 1.4 2017/10/04 20:57:10 patrick Exp $

LIB=	clangDriver
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${LLVM_SRCS}/tools/clang/lib/Driver

.include <bsd.own.mk>
SRCS=	AArch64.cpp \
	AMDGPU.cpp \
	ARM.cpp \
	AVR.cpp \
	Action.cpp \
	Ananas.cpp \
	BareMetal.cpp \
	Bitrig.cpp \
	Clang.cpp \
	CloudABI.cpp \
	CommonArgs.cpp \
	Compilation.cpp \
	Contiki.cpp \
	CrossWindows.cpp \
	Cuda.cpp \
	Darwin.cpp \
	Distro.cpp \
	DragonFly.cpp \
	Driver.cpp \
	DriverOptions.cpp \
	FreeBSD.cpp \
	Fuchsia.cpp \
	Gnu.cpp \
	Haiku.cpp \
	Hexagon.cpp \
	Job.cpp \
	Linux.cpp \
	MSVC.cpp \
	MinGW.cpp \
	Minix.cpp \
	Mips.cpp \
	MipsLinux.cpp \
	Multilib.cpp \
	Myriad.cpp \
	NaCl.cpp \
	NetBSD.cpp \
	OpenBSD.cpp \
	PPC.cpp \
	PS4CPU.cpp \
	Phases.cpp \
	SanitizerArgs.cpp \
	Solaris.cpp \
	Sparc.cpp \
	SystemZ.cpp \
	TCE.cpp \
	Tool.cpp \
	ToolChain.cpp \
	Types.cpp \
	WebAssembly.cpp \
	X86.cpp \
	XCore.cpp \
	XRayArgs.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Driver
.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Driver/ToolChains
.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Driver/ToolChains/Arch

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
