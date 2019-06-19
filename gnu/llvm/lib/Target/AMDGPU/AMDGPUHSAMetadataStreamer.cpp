//===--- AMDGPUHSAMetadataStreamer.cpp --------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
/// \file
/// AMDGPU HSA Metadata Streamer.
///
//
//===----------------------------------------------------------------------===//

#include "AMDGPUHSAMetadataStreamer.h"
#include "AMDGPU.h"
#include "AMDGPUSubtarget.h"
#include "SIMachineFunctionInfo.h"
#include "SIProgramInfo.h"
#include "Utils/AMDGPUBaseInfo.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {

static cl::opt<bool> DumpHSAMetadata(
    "amdgpu-dump-hsa-metadata",
    cl::desc("Dump AMDGPU HSA Metadata"));
static cl::opt<bool> VerifyHSAMetadata(
    "amdgpu-verify-hsa-metadata",
    cl::desc("Verify AMDGPU HSA Metadata"));

namespace AMDGPU {
namespace HSAMD {

void MetadataStreamer::dump(StringRef HSAMetadataString) const {
  errs() << "AMDGPU HSA Metadata:\n" << HSAMetadataString << '\n';
}

void MetadataStreamer::verify(StringRef HSAMetadataString) const {
  errs() << "AMDGPU HSA Metadata Parser Test: ";

  HSAMD::Metadata FromHSAMetadataString;
  if (fromString(HSAMetadataString, FromHSAMetadataString)) {
    errs() << "FAIL\n";
    return;
  }

  std::string ToHSAMetadataString;
  if (toString(FromHSAMetadataString, ToHSAMetadataString)) {
    errs() << "FAIL\n";
    return;
  }

  errs() << (HSAMetadataString == ToHSAMetadataString ? "PASS" : "FAIL")
         << '\n';
  if (HSAMetadataString != ToHSAMetadataString) {
    errs() << "Original input: " << HSAMetadataString << '\n'
           << "Produced output: " << ToHSAMetadataString << '\n';
  }
}

AccessQualifier MetadataStreamer::getAccessQualifier(StringRef AccQual) const {
  if (AccQual.empty())
    return AccessQualifier::Unknown;

  return StringSwitch<AccessQualifier>(AccQual)
             .Case("read_only",  AccessQualifier::ReadOnly)
             .Case("write_only", AccessQualifier::WriteOnly)
             .Case("read_write", AccessQualifier::ReadWrite)
             .Default(AccessQualifier::Default);
}

AddressSpaceQualifier MetadataStreamer::getAddressSpaceQualifer(
    unsigned AddressSpace) const {
  if (AddressSpace == AMDGPUASI.PRIVATE_ADDRESS)
    return AddressSpaceQualifier::Private;
  if (AddressSpace == AMDGPUASI.GLOBAL_ADDRESS)
    return AddressSpaceQualifier::Global;
  if (AddressSpace == AMDGPUASI.CONSTANT_ADDRESS)
    return AddressSpaceQualifier::Constant;
  if (AddressSpace == AMDGPUASI.LOCAL_ADDRESS)
    return AddressSpaceQualifier::Local;
  if (AddressSpace == AMDGPUASI.FLAT_ADDRESS)
    return AddressSpaceQualifier::Generic;
  if (AddressSpace == AMDGPUASI.REGION_ADDRESS)
    return AddressSpaceQualifier::Region;

  llvm_unreachable("Unknown address space qualifier");
}

ValueKind MetadataStreamer::getValueKind(Type *Ty, StringRef TypeQual,
                                         StringRef BaseTypeName) const {
  if (TypeQual.find("pipe") != StringRef::npos)
    return ValueKind::Pipe;

  return StringSwitch<ValueKind>(BaseTypeName)
             .Case("image1d_t", ValueKind::Image)
             .Case("image1d_array_t", ValueKind::Image)
             .Case("image1d_buffer_t", ValueKind::Image)
             .Case("image2d_t", ValueKind::Image)
             .Case("image2d_array_t", ValueKind::Image)
             .Case("image2d_array_depth_t", ValueKind::Image)
             .Case("image2d_array_msaa_t", ValueKind::Image)
             .Case("image2d_array_msaa_depth_t", ValueKind::Image)
             .Case("image2d_depth_t", ValueKind::Image)
             .Case("image2d_msaa_t", ValueKind::Image)
             .Case("image2d_msaa_depth_t", ValueKind::Image)
             .Case("image3d_t", ValueKind::Image)
             .Case("sampler_t", ValueKind::Sampler)
             .Case("queue_t", ValueKind::Queue)
             .Default(isa<PointerType>(Ty) ?
                          (Ty->getPointerAddressSpace() ==
                           AMDGPUASI.LOCAL_ADDRESS ?
                           ValueKind::DynamicSharedPointer :
                           ValueKind::GlobalBuffer) :
                      ValueKind::ByValue);
}

ValueType MetadataStreamer::getValueType(Type *Ty, StringRef TypeName) const {
  switch (Ty->getTypeID()) {
  case Type::IntegerTyID: {
    auto Signed = !TypeName.startswith("u");
    switch (Ty->getIntegerBitWidth()) {
    case 8:
      return Signed ? ValueType::I8 : ValueType::U8;
    case 16:
      return Signed ? ValueType::I16 : ValueType::U16;
    case 32:
      return Signed ? ValueType::I32 : ValueType::U32;
    case 64:
      return Signed ? ValueType::I64 : ValueType::U64;
    default:
      return ValueType::Struct;
    }
  }
  case Type::HalfTyID:
    return ValueType::F16;
  case Type::FloatTyID:
    return ValueType::F32;
  case Type::DoubleTyID:
    return ValueType::F64;
  case Type::PointerTyID:
    return getValueType(Ty->getPointerElementType(), TypeName);
  case Type::VectorTyID:
    return getValueType(Ty->getVectorElementType(), TypeName);
  default:
    return ValueType::Struct;
  }
}

std::string MetadataStreamer::getTypeName(Type *Ty, bool Signed) const {
  switch (Ty->getTypeID()) {
  case Type::IntegerTyID: {
    if (!Signed)
      return (Twine('u') + getTypeName(Ty, true)).str();

    auto BitWidth = Ty->getIntegerBitWidth();
    switch (BitWidth) {
    case 8:
      return "char";
    case 16:
      return "short";
    case 32:
      return "int";
    case 64:
      return "long";
    default:
      return (Twine('i') + Twine(BitWidth)).str();
    }
  }
  case Type::HalfTyID:
    return "half";
  case Type::FloatTyID:
    return "float";
  case Type::DoubleTyID:
    return "double";
  case Type::VectorTyID: {
    auto VecTy = cast<VectorType>(Ty);
    auto ElTy = VecTy->getElementType();
    auto NumElements = VecTy->getVectorNumElements();
    return (Twine(getTypeName(ElTy, Signed)) + Twine(NumElements)).str();
  }
  default:
    return "unknown";
  }
}

std::vector<uint32_t> MetadataStreamer::getWorkGroupDimensions(
    MDNode *Node) const {
  std::vector<uint32_t> Dims;
  if (Node->getNumOperands() != 3)
    return Dims;

  for (auto &Op : Node->operands())
    Dims.push_back(mdconst::extract<ConstantInt>(Op)->getZExtValue());
  return Dims;
}

Kernel::CodeProps::Metadata MetadataStreamer::getHSACodeProps(
    const MachineFunction &MF,
    const SIProgramInfo &ProgramInfo) const {
  const GCNSubtarget &STM = MF.getSubtarget<GCNSubtarget>();
  const SIMachineFunctionInfo &MFI = *MF.getInfo<SIMachineFunctionInfo>();
  HSAMD::Kernel::CodeProps::Metadata HSACodeProps;
  const Function &F = MF.getFunction();

  assert(F.getCallingConv() == CallingConv::AMDGPU_KERNEL ||
         F.getCallingConv() == CallingConv::SPIR_KERNEL);

  unsigned MaxKernArgAlign;
  HSACodeProps.mKernargSegmentSize = STM.getKernArgSegmentSize(F,
                                                               MaxKernArgAlign);
  HSACodeProps.mGroupSegmentFixedSize = ProgramInfo.LDSSize;
  HSACodeProps.mPrivateSegmentFixedSize = ProgramInfo.ScratchSize;
  HSACodeProps.mKernargSegmentAlign = std::max(MaxKernArgAlign, 4u);
  HSACodeProps.mWavefrontSize = STM.getWavefrontSize();
  HSACodeProps.mNumSGPRs = ProgramInfo.NumSGPR;
  HSACodeProps.mNumVGPRs = ProgramInfo.NumVGPR;
  HSACodeProps.mMaxFlatWorkGroupSize = MFI.getMaxFlatWorkGroupSize();
  HSACodeProps.mIsDynamicCallStack = ProgramInfo.DynamicCallStack;
  HSACodeProps.mIsXNACKEnabled = STM.isXNACKEnabled();
  HSACodeProps.mNumSpilledSGPRs = MFI.getNumSpilledSGPRs();
  HSACodeProps.mNumSpilledVGPRs = MFI.getNumSpilledVGPRs();

  return HSACodeProps;
}

Kernel::DebugProps::Metadata MetadataStreamer::getHSADebugProps(
    const MachineFunction &MF,
    const SIProgramInfo &ProgramInfo) const {
  const GCNSubtarget &STM = MF.getSubtarget<GCNSubtarget>();
  HSAMD::Kernel::DebugProps::Metadata HSADebugProps;

  if (!STM.debuggerSupported())
    return HSADebugProps;

  HSADebugProps.mDebuggerABIVersion.push_back(1);
  HSADebugProps.mDebuggerABIVersion.push_back(0);

  if (STM.debuggerEmitPrologue()) {
    HSADebugProps.mPrivateSegmentBufferSGPR =
        ProgramInfo.DebuggerPrivateSegmentBufferSGPR;
    HSADebugProps.mWavefrontPrivateSegmentOffsetSGPR =
        ProgramInfo.DebuggerWavefrontPrivateSegmentOffsetSGPR;
  }

  return HSADebugProps;
}

void MetadataStreamer::emitVersion() {
  auto &Version = HSAMetadata.mVersion;

  Version.push_back(VersionMajor);
  Version.push_back(VersionMinor);
}

void MetadataStreamer::emitPrintf(const Module &Mod) {
  auto &Printf = HSAMetadata.mPrintf;

  auto Node = Mod.getNamedMetadata("llvm.printf.fmts");
  if (!Node)
    return;

  for (auto Op : Node->operands())
    if (Op->getNumOperands())
      Printf.push_back(cast<MDString>(Op->getOperand(0))->getString());
}

void MetadataStreamer::emitKernelLanguage(const Function &Func) {
  auto &Kernel = HSAMetadata.mKernels.back();

  // TODO: What about other languages?
  auto Node = Func.getParent()->getNamedMetadata("opencl.ocl.version");
  if (!Node || !Node->getNumOperands())
    return;
  auto Op0 = Node->getOperand(0);
  if (Op0->getNumOperands() <= 1)
    return;

  Kernel.mLanguage = "OpenCL C";
  Kernel.mLanguageVersion.push_back(
      mdconst::extract<ConstantInt>(Op0->getOperand(0))->getZExtValue());
  Kernel.mLanguageVersion.push_back(
      mdconst::extract<ConstantInt>(Op0->getOperand(1))->getZExtValue());
}

void MetadataStreamer::emitKernelAttrs(const Function &Func) {
  auto &Attrs = HSAMetadata.mKernels.back().mAttrs;

  if (auto Node = Func.getMetadata("reqd_work_group_size"))
    Attrs.mReqdWorkGroupSize = getWorkGroupDimensions(Node);
  if (auto Node = Func.getMetadata("work_group_size_hint"))
    Attrs.mWorkGroupSizeHint = getWorkGroupDimensions(Node);
  if (auto Node = Func.getMetadata("vec_type_hint")) {
    Attrs.mVecTypeHint = getTypeName(
        cast<ValueAsMetadata>(Node->getOperand(0))->getType(),
        mdconst::extract<ConstantInt>(Node->getOperand(1))->getZExtValue());
  }
  if (Func.hasFnAttribute("runtime-handle")) {
    Attrs.mRuntimeHandle =
        Func.getFnAttribute("runtime-handle").getValueAsString().str();
  }
}

void MetadataStreamer::emitKernelArgs(const Function &Func) {
  for (auto &Arg : Func.args())
    emitKernelArg(Arg);

  emitHiddenKernelArgs(Func);
}

void MetadataStreamer::emitKernelArg(const Argument &Arg) {
  auto Func = Arg.getParent();
  auto ArgNo = Arg.getArgNo();
  const MDNode *Node;

  StringRef Name;
  Node = Func->getMetadata("kernel_arg_name");
  if (Node && ArgNo < Node->getNumOperands())
    Name = cast<MDString>(Node->getOperand(ArgNo))->getString();
  else if (Arg.hasName())
    Name = Arg.getName();

  StringRef TypeName;
  Node = Func->getMetadata("kernel_arg_type");
  if (Node && ArgNo < Node->getNumOperands())
    TypeName = cast<MDString>(Node->getOperand(ArgNo))->getString();

  StringRef BaseTypeName;
  Node = Func->getMetadata("kernel_arg_base_type");
  if (Node && ArgNo < Node->getNumOperands())
    BaseTypeName = cast<MDString>(Node->getOperand(ArgNo))->getString();

  StringRef AccQual;
  if (Arg.getType()->isPointerTy() && Arg.onlyReadsMemory() &&
      Arg.hasNoAliasAttr()) {
    AccQual = "read_only";
  } else {
    Node = Func->getMetadata("kernel_arg_access_qual");
    if (Node && ArgNo < Node->getNumOperands())
      AccQual = cast<MDString>(Node->getOperand(ArgNo))->getString();
  }

  StringRef TypeQual;
  Node = Func->getMetadata("kernel_arg_type_qual");
  if (Node && ArgNo < Node->getNumOperands())
    TypeQual = cast<MDString>(Node->getOperand(ArgNo))->getString();

  Type *Ty = Arg.getType();
  const DataLayout &DL = Func->getParent()->getDataLayout();

  unsigned PointeeAlign = 0;
  if (auto PtrTy = dyn_cast<PointerType>(Ty)) {
    if (PtrTy->getAddressSpace() == AMDGPUASI.LOCAL_ADDRESS) {
      PointeeAlign = Arg.getParamAlignment();
      if (PointeeAlign == 0)
        PointeeAlign = DL.getABITypeAlignment(PtrTy->getElementType());
    }
  }

  emitKernelArg(DL, Ty, getValueKind(Arg.getType(), TypeQual, BaseTypeName),
                PointeeAlign, Name, TypeName, BaseTypeName, AccQual, TypeQual);
}

void MetadataStreamer::emitKernelArg(const DataLayout &DL, Type *Ty,
                                     ValueKind ValueKind,
                                     unsigned PointeeAlign,
                                     StringRef Name,
                                     StringRef TypeName, StringRef BaseTypeName,
                                     StringRef AccQual, StringRef TypeQual) {
  HSAMetadata.mKernels.back().mArgs.push_back(Kernel::Arg::Metadata());
  auto &Arg = HSAMetadata.mKernels.back().mArgs.back();

  Arg.mName = Name;
  Arg.mTypeName = TypeName;
  Arg.mSize = DL.getTypeAllocSize(Ty);
  Arg.mAlign = DL.getABITypeAlignment(Ty);
  Arg.mValueKind = ValueKind;
  Arg.mValueType = getValueType(Ty, BaseTypeName);
  Arg.mPointeeAlign = PointeeAlign;

  if (auto PtrTy = dyn_cast<PointerType>(Ty))
    Arg.mAddrSpaceQual = getAddressSpaceQualifer(PtrTy->getAddressSpace());

  Arg.mAccQual = getAccessQualifier(AccQual);

  // TODO: Emit Arg.mActualAccQual.

  SmallVector<StringRef, 1> SplitTypeQuals;
  TypeQual.split(SplitTypeQuals, " ", -1, false);
  for (StringRef Key : SplitTypeQuals) {
    auto P = StringSwitch<bool*>(Key)
                 .Case("const",    &Arg.mIsConst)
                 .Case("restrict", &Arg.mIsRestrict)
                 .Case("volatile", &Arg.mIsVolatile)
                 .Case("pipe",     &Arg.mIsPipe)
                 .Default(nullptr);
    if (P)
      *P = true;
  }
}

void MetadataStreamer::emitHiddenKernelArgs(const Function &Func) {
  int HiddenArgNumBytes =
      getIntegerAttribute(Func, "amdgpu-implicitarg-num-bytes", 0);

  if (!HiddenArgNumBytes)
    return;

  auto &DL = Func.getParent()->getDataLayout();
  auto Int64Ty = Type::getInt64Ty(Func.getContext());

  if (HiddenArgNumBytes >= 8)
    emitKernelArg(DL, Int64Ty, ValueKind::HiddenGlobalOffsetX);
  if (HiddenArgNumBytes >= 16)
    emitKernelArg(DL, Int64Ty, ValueKind::HiddenGlobalOffsetY);
  if (HiddenArgNumBytes >= 24)
    emitKernelArg(DL, Int64Ty, ValueKind::HiddenGlobalOffsetZ);

  auto Int8PtrTy = Type::getInt8PtrTy(Func.getContext(),
                                      AMDGPUASI.GLOBAL_ADDRESS);

  // Emit "printf buffer" argument if printf is used, otherwise emit dummy
  // "none" argument.
  if (HiddenArgNumBytes >= 32) {
    if (Func.getParent()->getNamedMetadata("llvm.printf.fmts"))
      emitKernelArg(DL, Int8PtrTy, ValueKind::HiddenPrintfBuffer);
    else
      emitKernelArg(DL, Int8PtrTy, ValueKind::HiddenNone);
  }

  // Emit "default queue" and "completion action" arguments if enqueue kernel is
  // used, otherwise emit dummy "none" arguments.
  if (HiddenArgNumBytes >= 48) {
    if (Func.hasFnAttribute("calls-enqueue-kernel")) {
      emitKernelArg(DL, Int8PtrTy, ValueKind::HiddenDefaultQueue);
      emitKernelArg(DL, Int8PtrTy, ValueKind::HiddenCompletionAction);
    } else {
      emitKernelArg(DL, Int8PtrTy, ValueKind::HiddenNone);
      emitKernelArg(DL, Int8PtrTy, ValueKind::HiddenNone);
    }
  }
}

void MetadataStreamer::begin(const Module &Mod) {
  AMDGPUASI = getAMDGPUAS(Mod);
  emitVersion();
  emitPrintf(Mod);
}

void MetadataStreamer::end() {
  std::string HSAMetadataString;
  if (toString(HSAMetadata, HSAMetadataString))
    return;

  if (DumpHSAMetadata)
    dump(HSAMetadataString);
  if (VerifyHSAMetadata)
    verify(HSAMetadataString);
}

void MetadataStreamer::emitKernel(const MachineFunction &MF, const SIProgramInfo &ProgramInfo) {
  auto &Func = MF.getFunction();
  if (Func.getCallingConv() != CallingConv::AMDGPU_KERNEL)
    return;

  auto CodeProps = getHSACodeProps(MF, ProgramInfo);
  auto DebugProps = getHSADebugProps(MF, ProgramInfo);

  HSAMetadata.mKernels.push_back(Kernel::Metadata());
  auto &Kernel = HSAMetadata.mKernels.back();

  Kernel.mName = Func.getName();
  Kernel.mSymbolName = (Twine(Func.getName()) + Twine("@kd")).str();
  emitKernelLanguage(Func);
  emitKernelAttrs(Func);
  emitKernelArgs(Func);
  HSAMetadata.mKernels.back().mCodeProps = CodeProps;
  HSAMetadata.mKernels.back().mDebugProps = DebugProps;
}

} // end namespace HSAMD
} // end namespace AMDGPU
} // end namespace llvm
