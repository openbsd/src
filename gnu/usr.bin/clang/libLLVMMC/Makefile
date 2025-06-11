# $OpenBSD: Makefile,v 1.12 2025/06/11 17:46:32 robert Exp $

LIB=	LLVMMC
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/MC

SRCS+=	ConstantPools.cpp \
	DXContainerPSVInfo.cpp \
	ELFObjectWriter.cpp \
	GOFFObjectWriter.cpp \
	MCAsmBackend.cpp \
	MCAsmInfo.cpp \
	MCAsmInfoCOFF.cpp \
	MCAsmInfoDarwin.cpp \
	MCAsmInfoELF.cpp \
	MCAsmInfoGOFF.cpp \
	MCAsmInfoWasm.cpp \
	MCAsmInfoXCOFF.cpp \
	MCAsmMacro.cpp \
	MCAsmStreamer.cpp \
	MCAssembler.cpp \
	MCCodeEmitter.cpp \
	MCCodeView.cpp \
	MCContext.cpp \
	MCDwarf.cpp \
	MCDXContainerStreamer.cpp \
	MCDXContainerWriter.cpp \
	MCELFObjectTargetWriter.cpp \
	MCELFStreamer.cpp \
	MCExpr.cpp \
	MCFragment.cpp \
	MCGOFFStreamer.cpp \
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
	MCSectionDXContainer.cpp \
	MCSectionELF.cpp \
	MCSectionMachO.cpp \
	MCSectionWasm.cpp \
	MCSectionXCOFF.cpp \
	MCStreamer.cpp \
	MCSPIRVStreamer.cpp \
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
	SPIRVObjectWriter.cpp \
	StringTableBuilder.cpp \
	TargetRegistry.cpp \
	WasmObjectWriter.cpp \
	WinCOFFObjectWriter.cpp \
	XCOFFObjectWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/MC
