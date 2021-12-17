.include "../Makefile.arch"

.if "${LLVM_ARCH}" != ""

.include "../libLLVMAggressiveInstCombine/Makefile"
.include "../libLLVMAMDGPUAsmParser/Makefile"
.include "../libLLVMAMDGPUCodeGen/Makefile"
.include "../libLLVMAMDGPUDesc/Makefile"
.include "../libLLVMAMDGPUDisassembler/Makefile"
.include "../libLLVMAMDGPUInfo/Makefile"
.include "../libLLVMAMDGPUUtils/Makefile"
.include "../libLLVMAnalysis/Makefile"
.include "../libLLVMAsmParser/Makefile"
.include "../libLLVMAsmPrinter/Makefile"
.include "../libLLVMBinaryFormat/Makefile"
.include "../libLLVMBitReader/Makefile"
.include "../libLLVMBitWriter/Makefile"
.include "../libLLVMBitstreamReader/Makefile"
.include "../libLLVMCFGuard/Makefile"
.include "../libLLVMCodeGen/Makefile"
.include "../libLLVMCore/Makefile"
.include "../libLLVMCoroutines/Makefile"
.include "../libLLVMCoverage/Makefile"
.include "../libLLVMDebugInfoCodeView/Makefile"
.include "../libLLVMDebugInfoDWARF/Makefile"
.include "../libLLVMDebugInfoMSF/Makefile"
.include "../libLLVMDebugInfoPDB/Makefile"
.include "../libLLVMDemangle/Makefile"
.include "../libLLVMExecutionEngine/Makefile"
.include "../libLLVMFrontendOpenACC/Makefile"
.include "../libLLVMFrontendOpenMP/Makefile"
.include "../libLLVMGlobalISel/Makefile"
.include "../libLLVMIRReader/Makefile"
.include "../libLLVMInstCombine/Makefile"
.include "../libLLVMInstrumentation/Makefile"
.include "../libLLVMLTO/Makefile"
.include "../libLLVMLinker/Makefile"
.include "../libLLVMMC/Makefile"
.include "../libLLVMMCDisassembler/Makefile"
.include "../libLLVMMCParser/Makefile"
.include "../libLLVMMIRParser/Makefile"
.include "../libLLVMObjCARCOpts/Makefile"
.include "../libLLVMObject/Makefile"
.include "../libLLVMOption/Makefile"
.include "../libLLVMPasses/Makefile"
.include "../libLLVMProfileData/Makefile"
.include "../libLLVMRemarks/Makefile"
.include "../libLLVMScalarOpts/Makefile"
.include "../libLLVMSelectionDAG/Makefile"
.include "../libLLVMSupport/Makefile.shared"
.include "../libLLVMSymbolize/Makefile"
.include "../libLLVMTarget/Makefile"
.include "../libLLVMTextAPI/Makefile"
.include "../libLLVMTransformUtils/Makefile"
.include "../libLLVMVectorize/Makefile"
.include "../libLLVM${LLVM_ARCH}AsmParser/Makefile"
.include "../libLLVM${LLVM_ARCH}CodeGen/Makefile"
.include "../libLLVM${LLVM_ARCH}Desc/Makefile"
.include "../libLLVM${LLVM_ARCH}Disassembler/Makefile"
.include "../libLLVM${LLVM_ARCH}Info/Makefile"
.if "${LLVM_ARCH}" == "AArch64" || "${LLVM_ARCH}" == "ARM"
.include "../libLLVM${LLVM_ARCH}Utils/Makefile"
.endif
.include "../libLLVMipo/Makefile"

.endif

NOLIBSTATIC=	# do not install static version
LIB=	LLVM

includes:
	cd ${LLVM_SRCS}/include; \
	pax -rw -pa -L \
	    `find llvm llvm-c -follow -type f \
	    '(' -name '*.def' -or -name '*.h' -or -name '*.td' \
	    -or -name '*.inc' ')' \
	    -print` ${DESTDIR}/usr/include

.include <bsd.lib.mk>

# Enable return protectors for libLLVM.so
PICFLAG+= -fret-protector
