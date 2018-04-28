//===-- X86FixupGadgets.cpp - Fixup Instructions that make ROP Gadgets ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file
/// This file defines a function pass that checks instructions for sequences
/// that will lower to a potentially useful ROP gadget, and attempts to
/// replace those sequences with alternatives that are not useful for ROP.
///
//===----------------------------------------------------------------------===//

#include "X86.h"
#include "X86InstrBuilder.h"
#include "X86InstrInfo.h"
#include "X86MachineFunctionInfo.h"
#include "X86Subtarget.h"
#include "X86TargetMachine.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define FIXUPGADGETS_DESC "X86 ROP Gadget Fixup"
#define FIXUPGADGETS_NAME "x86-fixup-gadgets"

#define DEBUG_TYPE FIXUPGADGETS_NAME

// Toggle with cc1 option: -backend-option -x86-fixup-gadgets=<true|false>
static cl::opt<bool> FixupGadgets(
    "x86-fixup-gadgets",
    cl::desc("Replace ROP friendly instructions with alternatives"),
    cl::init(true));

namespace {
class FixupGadgetsPass : public MachineFunctionPass {

public:
  static char ID;

  StringRef getPassName() const override { return FIXUPGADGETS_DESC; }

  FixupGadgetsPass()
      : MachineFunctionPass(ID), STI(nullptr), TII(nullptr), TRI(nullptr) {}

  /// Loop over all the instructions and replace ROP friendly
  /// seuqences with less ROP friendly alternatives
  bool runOnMachineFunction(MachineFunction &MF) override;

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::NoVRegs);
  }

private:
  const X86Subtarget *STI;
  const X86InstrInfo *TII;
  const X86RegisterInfo *TRI;
  bool Is64Bit;
  enum InstrType {
    NoType = 0,
    OneGPRegC3,
    TwoGPRegC3,
    ThreeGPRegC3,
  };

  /// If an Instr has a ROP friendly construct, return it
  InstrType isROPFriendly(MachineInstr &MI) const;

  /// Helper functions for various kinds of instructions
  bool isOneGPRegC3(MachineInstr &MI) const;
  bool isTwoGPRegC3(MachineInstr &MI) const;
  bool isThreeGPRegC3(MachineInstr &MI) const;

  /// Replace ROP friendly instructions with safe alternatives
  bool fixupInstruction(MachineFunction &MF, MachineBasicBlock &MBB,
                        MachineInstr &MI, InstrType type);
};
char FixupGadgetsPass::ID = 0;
} // namespace

FunctionPass *llvm::createX86FixupGadgetsPass() {
  return new FixupGadgetsPass();
}

bool FixupGadgetsPass::isOneGPRegC3(MachineInstr &MI) const {
  MachineOperand &MO = MI.getOperand(0);
  return MO.isReg() && MO.getReg() == X86::EBX &&
         (MI.getDesc().TSFlags & X86II::FormMask) == X86II::MRMDestReg;
}

bool FixupGadgetsPass::isTwoGPRegC3(MachineInstr &MI) const {
  bool DestSet = false;
  bool SrcSet = false;
  bool OpcodeSet = false;
  MachineOperand &MO0 = MI.getOperand(0);
  MachineOperand &MO1 = MI.getOperand(1);
  if (!(MO0.isReg() && MO1.isReg()))
    return false;

  unsigned dstReg = MO0.getReg();
  if (dstReg == X86::RBX || dstReg == X86::EBX || dstReg == X86::BX ||
      dstReg == X86::BL)
    DestSet = true;

  if (!DestSet)
    return false;

  unsigned srcReg = MO1.getReg();
  if (srcReg == X86::RAX || srcReg == X86::EAX || srcReg == X86::AX ||
      srcReg == X86::AL)
    SrcSet = true;

  if (!SrcSet)
    return false;

  if ((MI.getDesc().TSFlags & X86II::FormMask) == X86II::MRMDestReg)
    OpcodeSet = true;

  return DestSet && SrcSet && OpcodeSet;
}

bool FixupGadgetsPass::isThreeGPRegC3(MachineInstr &MI) const {
  bool DestSet = false;
  bool SrcSet = false;
  bool OpcodeSet = false;

  MachineOperand &MO0 = MI.getOperand(0);
  MachineOperand &MO1 = MI.getOperand(1);
  MachineOperand &MO2 = MI.getOperand(2);
  if (!(MO0.isReg() && MO1.isReg() && MO2.isReg() &&
        MO0.getReg() == MO1.getReg()))
    return false;

  unsigned dstReg = MO0.getReg();
  if (dstReg == X86::RBX || dstReg == X86::EBX || dstReg == X86::BX ||
      dstReg == X86::BL)
    DestSet = true;

  if (!DestSet)
    return false;

  unsigned srcReg = MO2.getReg();
  if (srcReg == X86::RAX || srcReg == X86::EAX || srcReg == X86::AX ||
      srcReg == X86::AL)
    SrcSet = true;

  if (!SrcSet)
    return false;

  if ((MI.getDesc().TSFlags & X86II::FormMask) == X86II::MRMDestReg)
    OpcodeSet = true;

  return DestSet && SrcSet && OpcodeSet;
}

FixupGadgetsPass::InstrType
FixupGadgetsPass::isROPFriendly(MachineInstr &MI) const {
  switch (MI.getNumExplicitOperands()) {
  case 1:
    return isOneGPRegC3(MI) ? OneGPRegC3 : NoType;
  case 2:
    return isTwoGPRegC3(MI) ? TwoGPRegC3 : NoType;
  case 3:
    return isThreeGPRegC3(MI) ? ThreeGPRegC3 : NoType;
  }
  return NoType;
}

bool FixupGadgetsPass::fixupInstruction(MachineFunction &MF,
                                        MachineBasicBlock &MBB,
                                        MachineInstr &MI, InstrType type) {

  if (type == NoType)
    return false;

  MachineOperand *MO0, *MO1, *MO2;
  DebugLoc DL = MI.getDebugLoc();
  unsigned XCHG = Is64Bit ? X86::XCHG64rr : X86::XCHG32rr;
  unsigned SREG = Is64Bit ? X86::RAX : X86::EAX;
  unsigned DREG = Is64Bit ? X86::RBX : X86::EBX;
  unsigned tmpReg;

  // Swap the two registers to start
  BuildMI(MBB, MI, DL, TII->get(XCHG), DREG).addReg(DREG).addReg(SREG);

  switch (type) {
  case OneGPRegC3:
    MO0 = &MI.getOperand(0);
    switch (MO0->getReg()) {
    case X86::RBX:
      tmpReg = X86::RAX;
      break;
    case X86::EBX:
      tmpReg = X86::EAX;
      break;
    case X86::BX:
      tmpReg = X86::AX;
      break;
    case X86::BL:
      tmpReg = X86::AL;
      break;
    default:
      llvm_unreachable("Unknown DestReg in OneGPRegC3 fixup");
    }
    BuildMI(MBB, MI, DL, MI.getDesc(), tmpReg);
    break;
  case TwoGPRegC3:
    MO0 = &MI.getOperand(0);
    MO1 = &MI.getOperand(1);
    BuildMI(MBB, MI, DL, MI.getDesc(), MO1->getReg()).addReg(MO0->getReg());
    break;
  case ThreeGPRegC3:
    // Swap args around and set new dest reg
    MO0 = &MI.getOperand(0); // Destination
    MO2 = &MI.getOperand(2); // Source 2 == Other
    BuildMI(MBB, MI, DL, MI.getDesc(), MO2->getReg())
        .addReg(MO2->getReg())
        .addReg(MO0->getReg());
    break;
  default:
    llvm_unreachable("Unknown FixupGadgets Instruction Type");
  }

  // And swap them back to finish
  BuildMI(MBB, MI, DL, TII->get(XCHG), DREG).addReg(DREG).addReg(SREG);
  // Erase original instruction
  MI.eraseFromParent();

  return true;
}

bool FixupGadgetsPass::runOnMachineFunction(MachineFunction &MF) {
  if (!FixupGadgets)
    return false;

  STI = &MF.getSubtarget<X86Subtarget>();
  TII = STI->getInstrInfo();
  TRI = STI->getRegisterInfo();
  Is64Bit = STI->is64Bit();
  std::vector<std::pair<MachineInstr *, InstrType>> fixups;
  InstrType type;

  bool modified = false;

  for (auto &MBB : MF) {
    fixups.clear();
    for (auto &MI : MBB) {
      type = isROPFriendly(MI);
      if (type != NoType)
        fixups.push_back(std::make_pair(&MI, type));
    }
    for (auto &fixup : fixups)
      modified |= fixupInstruction(MF, MBB, *fixup.first, fixup.second);
  }

  return modified;
}
