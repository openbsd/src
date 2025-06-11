# $OpenBSD: Makefile,v 1.12 2025/06/11 17:46:33 robert Exp $

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
	MultilibBuilder.cpp \
	OffloadBundler.cpp \
	OptionUtils.cpp \
	Phases.cpp \
	SanitizerArgs.cpp \
	Tool.cpp \
	ToolChain.cpp \
	Types.cpp \
	XRayArgs.cpp

# Driver/Toolchains
SRCS+=	AIX.cpp \
	AMDGPU.cpp \
	AMDGPUOpenMP.cpp \
	AVR.cpp \
	BareMetal.cpp \
	Clang.cpp \
	CommonArgs.cpp \
	CrossWindows.cpp \
	CSKYToolChain.cpp \
	Cuda.cpp \
	Darwin.cpp \
	DragonFly.cpp \
	Flang.cpp \
	FreeBSD.cpp \
	Fuchsia.cpp \
	Gnu.cpp \
	Haiku.cpp \
	HIPUtility.cpp \
	HIPAMD.cpp \
	HIPSPV.cpp \
	Hexagon.cpp \
	HLSL.cpp \
	Hurd.cpp \
	Linux.cpp \
	MipsLinux.cpp \
	MinGW.cpp \
	MSP430.cpp \
	MSVC.cpp \
	NaCl.cpp \
	NetBSD.cpp \
	OHOS.cpp \
	OpenBSD.cpp \
	PS4CPU.cpp \
	RISCVToolchain.cpp \
	Solaris.cpp \
	SPIRV.cpp \
	TCE.cpp \
	VEToolchain.cpp \
	WebAssembly.cpp \
	XCore.cpp \
	PPCLinux.cpp \
	PPCFreeBSD.cpp \
	InterfaceStubs.cpp \
	ZOS.cpp

# Driver/Toolchains/Arch
SRCS+=	AArch64.cpp \
	ARM.cpp \
	CSKY.cpp \
	LoongArch.cpp \
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
