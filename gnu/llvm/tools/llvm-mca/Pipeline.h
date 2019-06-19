//===--------------------- Pipeline.h ---------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file
///
/// This file implements an ordered container of stages that simulate the
/// pipeline of a hardware backend.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_TOOLS_LLVM_MCA_PIPELINE_H
#define LLVM_TOOLS_LLVM_MCA_PIPELINE_H

#include "Scheduler.h"
#include "Stage.h"
#include "llvm/ADT/SmallVector.h"

namespace mca {

class HWEventListener;
class HWInstructionEvent;
class HWStallEvent;

/// A pipeline for a specific subtarget.
///
/// It emulates an out-of-order execution of instructions. Instructions are
/// fetched from a MCInst sequence managed by an initial 'Fetch' stage.
/// Instructions are firstly fetched, then dispatched to the schedulers, and
/// then executed.
///
/// This class tracks the lifetime of an instruction from the moment where
/// it gets dispatched to the schedulers, to the moment where it finishes
/// executing and register writes are architecturally committed.
/// In particular, it monitors changes in the state of every instruction
/// in flight.
///
/// Instructions are executed in a loop of iterations. The number of iterations
/// is defined by the SourceMgr object, which is managed by the initial stage
/// of the instruction pipeline.
///
/// The Pipeline entry point is method 'run()' which executes cycles in a loop
/// until there are new instructions to dispatch, and not every instruction
/// has been retired.
///
/// Internally, the Pipeline collects statistical information in the form of
/// histograms. For example, it tracks how the dispatch group size changes
/// over time.
class Pipeline {
  Pipeline(const Pipeline &P) = delete;
  Pipeline &operator=(const Pipeline &P) = delete;

  /// An ordered list of stages that define this instruction pipeline.
  llvm::SmallVector<std::unique_ptr<Stage>, 8> Stages;
  std::set<HWEventListener *> Listeners;
  unsigned Cycles;

  void preExecuteStages();
  bool executeStages(InstRef &IR);
  void postExecuteStages();
  void runCycle();

  bool hasWorkToProcess();
  void notifyCycleBegin();
  void notifyCycleEnd();

public:
  Pipeline() : Cycles(0) {}
  void appendStage(std::unique_ptr<Stage> S) { Stages.push_back(std::move(S)); }
  void run();
  void addEventListener(HWEventListener *Listener);
};
} // namespace mca

#endif // LLVM_TOOLS_LLVM_MCA_PIPELINE_H
