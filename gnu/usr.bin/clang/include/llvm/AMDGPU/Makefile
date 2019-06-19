# $OpenBSD: Makefile,v 1.1 2019/01/29 00:18:23 jsg Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../llvm

HDRS=	AMDGPUGenAsmMatcher.inc AMDGPUGenAsmWriter.inc \
	AMDGPUGenCallingConv.inc AMDGPUGenDAGISel.inc \
	AMDGPUGenDisassemblerTables.inc AMDGPUGenInstrInfo.inc \
	AMDGPUGenIntrinsicEnums.inc AMDGPUGenIntrinsicImpl.inc \
	AMDGPUGenMCCodeEmitter.inc AMDGPUGenMCPseudoLowering.inc \
	AMDGPUGenRegisterBank.inc AMDGPUGenRegisterInfo.inc \
	AMDGPUGenSearchableTables.inc AMDGPUGenSubtargetInfo.inc

HDRS+=	AMDGPUGenGlobalISel.inc

HDRS+=	R600GenAsmWriter.inc R600GenCallingConv.inc \
	R600GenDAGISel.inc R600GenDFAPacketizer.inc \
	R600GenInstrInfo.inc R600GenMCCodeEmitter.inc \
	R600GenRegisterInfo.inc R600GenSubtargetInfo.inc

all: ${HDRS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${HDRS}

AMDGPUGenAsmMatcher.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/AMDGPU.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-asm-matcher \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
AMDGPUGenAsmWriter.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/AMDGPU.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-asm-writer \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
AMDGPUGenCallingConv.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/AMDGPU.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-callingconv \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
AMDGPUGenDAGISel.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/AMDGPU.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-dag-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
AMDGPUGenDisassemblerTables.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/AMDGPU.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-disassembler \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
AMDGPUGenInstrInfo.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/AMDGPU.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-instr-info \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
AMDGPUGenIntrinsicEnums.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/AMDGPU.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-tgt-intrinsic-enums \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
AMDGPUGenIntrinsicImpl.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/AMDGPU.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-tgt-intrinsic-impl \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
AMDGPUGenMCCodeEmitter.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/AMDGPU.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-emitter \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
AMDGPUGenMCPseudoLowering.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/AMDGPU.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-pseudo-lowering \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
AMDGPUGenRegisterBank.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/AMDGPU.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-register-bank \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
AMDGPUGenRegisterInfo.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/AMDGPU.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-register-info \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
AMDGPUGenSearchableTables.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/AMDGPU.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-searchable-tables \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
AMDGPUGenSubtargetInfo.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/AMDGPU.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-subtarget \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}

AMDGPUGenGlobalISel.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/AMDGPUGISel.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-global-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}

R600GenAsmWriter.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/R600.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-asm-writer \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
R600GenCallingConv.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/R600.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-callingconv \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
R600GenDAGISel.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/R600.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-dag-isel \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
R600GenDFAPacketizer.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/R600.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-dfa-packetizer \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
R600GenInstrInfo.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/R600.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-instr-info \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
R600GenMCCodeEmitter.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/R600.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-emitter \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
R600GenRegisterInfo.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/R600.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-register-info \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}
R600GenSubtargetInfo.inc: ${LLVM_SRCS}/lib/Target/AMDGPU/R600.td
	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen -gen-subtarget \
		-I${LLVM_SRCS}/include -I${LLVM_SRCS}/lib/Target/AMDGPU \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
