# $OpenBSD: Makefile,v 1.5 2018/04/06 14:44:04 patrick Exp $

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
	MCCodeView.cpp \
	MCCodePadder.cpp \
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
	MCSectionWasm.cpp \
	MCStreamer.cpp \
	MCSubtargetInfo.cpp \
	MCSymbol.cpp \
	MCSymbolELF.cpp \
	MCTargetOptions.cpp \
	MCValue.cpp \
	MCWasmStreamer.cpp \
	MCWin64EH.cpp \
	MCWinCOFFStreamer.cpp \
	MCWinEH.cpp \
	MachObjectWriter.cpp \
	StringTableBuilder.cpp \
	SubtargetFeature.cpp \
	WinCOFFObjectWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/MC

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
