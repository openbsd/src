//===--------------------- PipelinePrinter.cpp ------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file
///
/// This file implements the PipelinePrinter interface.
///
//===----------------------------------------------------------------------===//

#include "PipelinePrinter.h"
#include "View.h"

namespace mca {

using namespace llvm;

void PipelinePrinter::printReport(llvm::raw_ostream &OS) const {
  for (const auto &V : Views)
    V->printView(OS);
}
} // namespace mca.
