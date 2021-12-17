# $OpenBSD: Makefile,v 1.10 2021/12/17 14:55:45 patrick Exp $

LIB=	LLVMMC
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/MC

SRCS+=	ConstantPools.cpp \
	ELFObjectWriter.cpp \
	MCAsmBackend.cpp \
	MCAsmInfo.cpp \
	MCAsmInfoCOFF.cpp \
	MCAsmInfoDarwin.cpp \
	MCAsmInfoELF.cpp \
	MCAsmInfoWasm.cpp \
	MCAsmInfoXCOFF.cpp \
	MCAsmMacro.cpp \
	MCAsmStreamer.cpp \
	MCAssembler.cpp \
	MCCodeEmitter.cpp \
	MCCodeView.cpp \
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
	MCInstrInfo.cpp \
	MCLabel.cpp \
	MCLinkerOptimizationHint.cpp \
	MCMachOStreamer.cpp \
	MCMachObjectTargetWriter.cpp \
	MCNullStreamer.cpp \
	MCObjectFileInfo.cpp \
	MCObjectStreamer.cpp \
	MCObjectWriter.cpp \
	MCPseudoProbe.cpp \
	MCRegisterInfo.cpp \
	MCSchedule.cpp \
	MCSection.cpp \
	MCSectionCOFF.cpp \
	MCSectionELF.cpp \
	MCSectionMachO.cpp \
	MCSectionWasm.cpp \
	MCSectionXCOFF.cpp \
	MCStreamer.cpp \
	MCSubtargetInfo.cpp \
	MCSymbol.cpp \
	MCSymbolELF.cpp \
	MCSymbolXCOFF.cpp \
	MCTargetOptions.cpp \
	MCTargetOptionsCommandFlags.cpp \
	MCValue.cpp \
	MCWasmObjectTargetWriter.cpp \
	MCWasmStreamer.cpp \
	MCWin64EH.cpp \
	MCWinCOFFStreamer.cpp \
	MCWinEH.cpp \
	MCXCOFFObjectTargetWriter.cpp \
	MCXCOFFStreamer.cpp \
	MachObjectWriter.cpp \
	StringTableBuilder.cpp \
	SubtargetFeature.cpp \
	WasmObjectWriter.cpp \
	WinCOFFObjectWriter.cpp \
	XCOFFObjectWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/MC
