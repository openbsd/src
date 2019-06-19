//===-- MCInstrDescView.h ---------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Provide views around LLVM structures to represents an instruction instance,
/// as well as its implicit and explicit arguments in a uniform way.
/// Arguments that are explicit and independant (non tied) also have a Variable
/// associated to them so the instruction can be fully defined by reading its
/// Variables.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_TOOLS_LLVM_EXEGESIS_MCINSTRDESCVIEW_H
#define LLVM_TOOLS_LLVM_EXEGESIS_MCINSTRDESCVIEW_H

#include <random>

#include "RegisterAliasing.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/Optional.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/MC/MCInstrInfo.h"

namespace exegesis {

struct Operand; // forward declaration.

// A variable represents the value associated to an Operand or a set of Operands
// if they are tied together.
struct Variable {
  // The indices of the operands tied to this Variable.
  llvm::SmallVector<unsigned, 2> TiedOperands;
  llvm::MCOperand AssignedValue;
  // The index of this Variable in Instruction.Variables and its associated
  // Value in InstructionInstance.VariableValues.
  unsigned Index = -1;
};

// MCOperandInfo can only represents Explicit operands. This object gives a
// uniform view of Implicit and Explicit Operands.
//
// - Index: can be used to refer to MCInstrDesc::operands for Explicit operands.
// - Tracker: is set for Register Operands and is used to keep track of possible
// registers and the registers reachable from them (aliasing registers).
// - Info: a shortcut for MCInstrDesc::operands()[Index].
// - TiedToIndex: the index of the Operand holding the value or -1.
// - ImplicitReg: a pointer to the register value when Operand is Implicit,
// nullptr otherwise.
// - VariableIndex: the index of the Variable holding the value for this Operand
// or -1 if this operand is implicit.
struct Operand {
  unsigned Index = 0;
  bool IsDef = false;
  bool IsExplicit = false;
  const RegisterAliasingTracker *Tracker = nullptr; // Set for Register Op.
  const llvm::MCOperandInfo *Info = nullptr;        // Set for Explicit Op.
  int TiedToIndex = -1;                             // Set for Reg&Explicit Op.
  const llvm::MCPhysReg *ImplicitReg = nullptr;     // Set for Implicit Op.
  int VariableIndex = -1;                           // Set for Explicit Op.
};

// A view over an MCInstrDesc offering a convenient interface to compute
// Register aliasing.
struct Instruction {
  Instruction(const llvm::MCInstrDesc &MCInstrDesc,
              const RegisterAliasingTrackerCache &ATC);

  const llvm::MCInstrDesc *Description; // Never nullptr.
  llvm::SmallVector<Operand, 8> Operands;
  llvm::SmallVector<Variable, 4> Variables;
  llvm::BitVector DefRegisters; // The union of the aliased def registers.
  llvm::BitVector UseRegisters; // The union of the aliased use registers.
};

// An instance of an Instruction holding values for each of its Variables.
struct InstructionInstance {
  InstructionInstance(const Instruction &Instr);

  // No copy.
  InstructionInstance(const InstructionInstance &) = delete;
  InstructionInstance &operator=(const InstructionInstance &) = delete;

  // Moving is OK.
  InstructionInstance(InstructionInstance &&);
  InstructionInstance &operator=(InstructionInstance &&);

  unsigned getOpcode() const;
  llvm::MCOperand &getValueFor(const Variable &Var);
  const llvm::MCOperand &getValueFor(const Variable &Var) const;
  llvm::MCOperand &getValueFor(const Operand &Op);
  const llvm::MCOperand &getValueFor(const Operand &Op) const;
  bool hasImmediateVariables() const;

  // Assigns a Random Value to all Variables that are still Invalid.
  void randomizeUnsetVariables();

  // Returns the instance as an llvm::MCInst. The InstructionInstance must be
  // fully allocated (no invalid variables).
  llvm::MCInst build() const;

  Instruction Instr;
  llvm::SmallVector<llvm::MCOperand, 4> VariableValues;
};

// A prototype is a set of InstructionInstances with an explanation of how
// it's been built. The prototype can then be randomized to exercice several
// immediate values. It is also used to gather the used registers and define
// their initial values.
struct SnippetPrototype {
  SnippetPrototype() = default;

  // No copy.
  SnippetPrototype(const SnippetPrototype &) = delete;
  SnippetPrototype &operator=(const SnippetPrototype &) = delete;

  // Moving is OK.
  SnippetPrototype(SnippetPrototype &&);
  SnippetPrototype &operator=(SnippetPrototype &&);

  std::string Explanation;
  std::vector<InstructionInstance> Snippet;
};

// Represents the assignment of a Register to an Operand.
struct RegisterOperandAssignment {
  RegisterOperandAssignment(const Operand *Operand, llvm::MCPhysReg Reg)
      : Op(Operand), Reg(Reg) {}

  const Operand *Op; // Pointer to an Explicit Register Operand.
  llvm::MCPhysReg Reg;

  bool operator==(const RegisterOperandAssignment &other) const;
};

// Represents a set of Operands that would alias through the use of some
// Registers.
// There are two reasons why operands would alias:
// - The registers assigned to each of the operands are the same or alias each
//   other (e.g. AX/AL)
// - The operands are tied.
struct AliasingRegisterOperands {
  llvm::SmallVector<RegisterOperandAssignment, 1> Defs; // Unlikely size() > 1.
  llvm::SmallVector<RegisterOperandAssignment, 2> Uses;

  // True is Defs and Use contain an Implicit Operand.
  bool hasImplicitAliasing() const;

  bool operator==(const AliasingRegisterOperands &other) const;
};

// Returns all possible configurations leading Def registers of DefInstruction
// to alias with Use registers of UseInstruction.
struct AliasingConfigurations {
  AliasingConfigurations(const Instruction &DefInstruction,
                         const Instruction &UseInstruction);

  bool empty() const; // True if no aliasing configuration is found.
  bool hasImplicitAliasing() const;
  void setExplicitAliasing() const;

  const Instruction &DefInstruction;
  const Instruction &UseInstruction;
  llvm::SmallVector<AliasingRegisterOperands, 32> Configurations;
};

// A global Random Number Generator to randomize configurations.
// FIXME: Move random number generation into an object and make it seedable for
// unit tests.
std::mt19937 &randomGenerator();

// Picks a random bit among the bits set in Vector and returns its index.
// Precondition: Vector must have at least one bit set.
size_t randomBit(const llvm::BitVector &Vector);

// Picks a random configuration, then selects a random def and a random use from
// it and finally set the selected values in the provided InstructionInstances.
void setRandomAliasing(const AliasingConfigurations &AliasingConfigurations,
                       InstructionInstance &DefII, InstructionInstance &UseII);

// Writes MCInst to OS.
// This is not assembly but the internal LLVM's name for instructions and
// registers.
void DumpMCInst(const llvm::MCRegisterInfo &MCRegisterInfo,
                const llvm::MCInstrInfo &MCInstrInfo,
                const llvm::MCInst &MCInst, llvm::raw_ostream &OS);

} // namespace exegesis

#endif // LLVM_TOOLS_LLVM_EXEGESIS_MCINSTRDESCVIEW_H
