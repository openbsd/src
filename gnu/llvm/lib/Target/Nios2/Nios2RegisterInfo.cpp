//===-- Nios2RegisterInfo.cpp - Nios2 Register Information -== ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the Nios2 implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "nios2-reg-info"

#include "Nios2RegisterInfo.h"

#include "Nios2.h"
#include "Nios2Subtarget.h"

#define GET_REGINFO_TARGET_DESC
#include "Nios2GenRegisterInfo.inc"

using namespace llvm;

Nios2RegisterInfo::Nios2RegisterInfo(const Nios2Subtarget &ST)
    : Nios2GenRegisterInfo(Nios2::RA), Subtarget(ST) {}

const TargetRegisterClass *Nios2RegisterInfo::intRegClass(unsigned Size) const {
  return &Nios2::CPURegsRegClass;
}

const MCPhysReg *
Nios2RegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  return CSR_SaveList;
}

BitVector Nios2RegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  static const MCPhysReg ReservedCPURegs[] = {Nios2::ZERO, Nios2::AT, Nios2::SP,
                                             Nios2::RA,   Nios2::PC, Nios2::GP};
  BitVector Reserved(getNumRegs());

  for (unsigned I = 0; I < array_lengthof(ReservedCPURegs); ++I)
    Reserved.set(ReservedCPURegs[I]);

  return Reserved;
}

void Nios2RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                            int SPAdj, unsigned FIOperandNum,
                                            RegScavenger *RS) const {}

unsigned Nios2RegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return Nios2::SP;
}
