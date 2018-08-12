//===- ReturnProtectorLowering.cpp - ---------------------------------------==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Implements common routines for return protector support.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/ReturnProtectorLowering.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/IR/Function.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

using namespace llvm;

static void markUsedRegsInSuccessors(MachineBasicBlock &MBB,
                                     SmallSet<unsigned, 16> &Used,
                                     SmallSet<int, 24> &Visited) {
  int BBNum = MBB.getNumber();
  if (Visited.count(BBNum))
    return;

  // Mark all the registers used
  for (auto &MBBI : MBB.instrs()) {
    for (auto &MBBIOp : MBBI.operands()) {
      if (MBBIOp.isReg())
        Used.insert(MBBIOp.getReg());
    }
  }

  // Mark this MBB as visited
  Visited.insert(BBNum);
  // Recurse over all successors
  for (auto &SuccMBB : MBB.successors())
    markUsedRegsInSuccessors(*SuccMBB, Used, Visited);
}

/// setupReturnProtector - Checks the function for ROP friendly return
/// instructions and sets ReturnProtectorNeeded if found.
void ReturnProtectorLowering::setupReturnProtector(MachineFunction &MF) const {
  if (MF.getFunction().hasFnAttribute("ret-protector")) {
    for (auto &MBB : MF) {
      for (auto &T : MBB.terminators()) {
        if (opcodeIsReturn(T.getOpcode())) {
          MF.getFrameInfo().setReturnProtectorNeeded(true);
          return;
        }
      }
    }
  }
}

/// saveReturnProtectorRegister - Allows the target to save the
/// ReturnProtectorRegister in the CalleeSavedInfo vector if needed.
void ReturnProtectorLowering::saveReturnProtectorRegister(
    const MachineFunction &MF, std::vector<CalleeSavedInfo> &CSI) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  if (!MFI.getReturnProtectorNeeded())
    return;

  if (!MFI.hasReturnProtectorRegister())
    llvm_unreachable("Saving unset return protector register");

  CSI.push_back(CalleeSavedInfo(MFI.getReturnProtectorRegister()));
}

/// determineReturnProtectorTempRegister - Find a register that can be used
/// during function prologue / epilogue to store the return protector cookie.
/// Returns false if a register is needed but could not be found,
/// otherwise returns true.
bool ReturnProtectorLowering::determineReturnProtectorRegister(
    MachineFunction &MF, const SmallVector<MachineBasicBlock *, 4> &SaveBlocks,
    const SmallVector<MachineBasicBlock *, 4> &RestoreBlocks) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  if (!MFI.getReturnProtectorNeeded())
    return true;

  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();

  SmallSet<unsigned, 16> Used;
  SmallSet<int, 24> Visited;

  // CSR spills happen at the beginning of this block
  // so we can mark it as visited because anything past it is safe
  for (auto &SB : SaveBlocks)
    Visited.insert(SB->getNumber());

  // CSR Restores happen at the end of restore blocks, before any terminators,
  // so we need to search restores for MBB terminators, and any successor BBs.
  for (auto &RB : RestoreBlocks) {
    for (auto &RBI : RB->terminators()) {
      for (auto &RBIOp : RBI.operands()) {
        if (RBIOp.isReg())
          Used.insert(RBIOp.getReg());
      }
    }
    for (auto &SuccMBB : RB->successors())
      markUsedRegsInSuccessors(*SuccMBB, Used, Visited);
  }

  // Now we iterate from the front to find code paths that
  // bypass save blocks and land on return blocks
  markUsedRegsInSuccessors(MF.front(), Used, Visited);

  // Now we have gathered all the regs used outside the frame save / restore,
  // so we can see if we have a free reg to use for the retguard cookie.
  std::vector<unsigned> TempRegs;
  fillTempRegisters(MF, TempRegs);

  for (unsigned Reg : TempRegs) {
    bool canUse = true;
    for (MCRegAliasIterator AI(Reg, TRI, true); AI.isValid(); ++AI) {
      if (Used.count(*AI)) {
        // Reg is used somewhere, so we cannot use it
        canUse = false;
        break;
      }
    }
    if (canUse) {
      MFI.setReturnProtectorRegister(Reg);
      break;
    }
  }

  return MFI.hasReturnProtectorRegister();
}

/// insertReturnProtectors - insert return protector instrumentation.
void ReturnProtectorLowering::insertReturnProtectors(
    MachineFunction &MF) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();

  if (!MFI.getReturnProtectorNeeded())
    return;

  if (!MFI.hasReturnProtectorRegister())
    llvm_unreachable("Inconsistent return protector state.");

  const Function &Fn = MF.getFunction();
  const Module *M = Fn.getParent();
  GlobalVariable *cookie =
      dyn_cast_or_null<GlobalVariable>(M->getGlobalVariable(
          Fn.getFnAttribute("ret-protector-cookie").getValueAsString(),
          Type::getInt8PtrTy(M->getContext())));

  if (!cookie)
    llvm_unreachable("Function needs return protector but no cookie assigned");

  std::vector<MachineInstr *> returns;
  for (auto &MBB : MF) {
    if (MBB.isReturnBlock()) {
      for (auto &MI : MBB.terminators()) {
        if (opcodeIsReturn(MI.getOpcode()))
          returns.push_back(&MI);
      }
    }
  }

  if (returns.empty())
    return;

  for (auto &MI : returns)
    insertReturnProtectorEpilogue(MF, *MI, cookie);

  insertReturnProtectorPrologue(MF, MF.front(), cookie);
}
