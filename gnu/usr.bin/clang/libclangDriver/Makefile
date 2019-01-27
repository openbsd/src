# $OpenBSD: Makefile,v 1.6 2019/01/27 16:59:42 patrick Exp $

LIB=	clangDriver
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${LLVM_SRCS}/tools/clang/lib/Driver

.include <bsd.own.mk>
SRCS=	AMDGPU.cpp \
	AVR.cpp \
	Action.cpp \
	Ananas.cpp \
	BareMetal.cpp \
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
	HIP.cpp \
	Haiku.cpp \
	Hexagon.cpp \
	Job.cpp \
	Linux.cpp \
	MSVC.cpp \
	MinGW.cpp \
	Minix.cpp \
	MipsLinux.cpp \
	Multilib.cpp \
	Myriad.cpp \
	NaCl.cpp \
	NetBSD.cpp \
	OpenBSD.cpp \
	PS4CPU.cpp \
	Phases.cpp \
	RISCV.cpp \
	SanitizerArgs.cpp \
	Solaris.cpp \
	TCE.cpp \
	Tool.cpp \
	ToolChain.cpp \
	Types.cpp \
	WebAssembly.cpp \
	XCore.cpp \
	XRayArgs.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Driver
.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Driver/ToolChains

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
