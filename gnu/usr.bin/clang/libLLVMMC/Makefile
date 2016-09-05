# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

LIB=	LLVMMC
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/MC

.include <bsd.own.mk>
SRCS=	ConstantPools.cpp \
	ELFObjectWriter.cpp \
	MCAsmBackend.cpp \
	MCAsmInfo.cpp \
	MCAsmInfoCOFF.cpp \
	MCAsmInfoDarwin.cpp \
	MCAsmInfoELF.cpp \
	MCAsmStreamer.cpp \
	MCAssembler.cpp \
	MCCodeEmitter.cpp \
	MCCodeGenInfo.cpp \
	MCContext.cpp \
	MCDwarf.cpp \
	MCELFObjectTargetWriter.cpp \
	MCELFStreamer.cpp \
	MCExpr.cpp \
	MCFragment.cpp \
	MCInst.cpp \
	MCInstPrinter.cpp \
	MCInstrAnalysis.cpp \
	MCInstrDesc.cpp \
	MCLabel.cpp \
	MCLinkerOptimizationHint.cpp \
	MCMachOStreamer.cpp \
	MCMachObjectTargetWriter.cpp \
	MCNullStreamer.cpp \
	MCObjectFileInfo.cpp \
	MCObjectStreamer.cpp \
	MCObjectWriter.cpp \
	MCRegisterInfo.cpp \
	MCSchedule.cpp \
	MCSection.cpp \
	MCSectionCOFF.cpp \
	MCSectionELF.cpp \
	MCSectionMachO.cpp \
	MCStreamer.cpp \
	MCSubtargetInfo.cpp \
	MCSymbol.cpp \
	MCSymbolELF.cpp \
	MCSymbolizer.cpp \
	MCTargetOptions.cpp \
	MCValue.cpp \
	MCWin64EH.cpp \
	MCWinEH.cpp \
	MachObjectWriter.cpp \
	StringTableBuilder.cpp \
	SubtargetFeature.cpp \
	WinCOFFObjectWriter.cpp \
	WinCOFFStreamer.cpp \
	YAML.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/MC

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
