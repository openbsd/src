# $OpenBSD: Makefile,v 1.10 2021/12/17 14:55:46 patrick Exp $

LIB=	clangDriver
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${CLANG_SRCS}/lib/Driver

.include <bsd.own.mk>

# Driver
SRCS=	Action.cpp \
	Compilation.cpp \
	Distro.cpp \
	Driver.cpp \
	DriverOptions.cpp \
	Job.cpp \
	Multilib.cpp \
	OptionUtils.cpp \
	Phases.cpp \
	SanitizerArgs.cpp \
	Tool.cpp \
	ToolChain.cpp \
	Types.cpp \
	XRayArgs.cpp

# Driver/Toolchains
SRCS+=	AIX.cpp \
	Ananas.cpp \
	AMDGPU.cpp \
	AMDGPUOpenMP.cpp \
	AVR.cpp \
	BareMetal.cpp \
	Clang.cpp \
	CloudABI.cpp \
	CommonArgs.cpp \
	Contiki.cpp \
	CrossWindows.cpp \
	Cuda.cpp \
	Darwin.cpp \
	DragonFly.cpp \
	Flang.cpp \
	FreeBSD.cpp \
	Fuchsia.cpp \
	Gnu.cpp \
	Haiku.cpp \
	HIP.cpp \
	Hexagon.cpp \
	Hurd.cpp \
	Linux.cpp \
	MipsLinux.cpp \
	MinGW.cpp \
	Minix.cpp \
	MSP430.cpp \
	MSVC.cpp \
	Myriad.cpp \
	NaCl.cpp \
	NetBSD.cpp \
	OpenBSD.cpp \
	PS4CPU.cpp \
	RISCVToolchain.cpp \
	Solaris.cpp \
	TCE.cpp \
	VEToolchain.cpp \
	WebAssembly.cpp \
	XCore.cpp \
	PPCLinux.cpp \
	InterfaceStubs.cpp \
	ZOS.cpp

# Driver/Toolchains/Arch
SRCS+=	AArch64.cpp \
	ARM.cpp \
	M68k.cpp \
	Mips.cpp \
	PPC.cpp \
	RISCV.cpp \
	Sparc.cpp \
	SystemZ.cpp \
	VE.cpp \
	X86.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Driver
.PATH:	${.CURDIR}/../../../llvm/clang/lib/Driver/ToolChains
.PATH:	${.CURDIR}/../../../llvm/clang/lib/Driver/ToolChains/Arch

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
