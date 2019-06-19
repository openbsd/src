//===-- RegisterContextMach_i386.cpp ----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#if defined(__APPLE__)

// C Includes
#include <mach/thread_act.h>

// C++ Includes
// Other libraries and framework includes
// Project includes
#include "RegisterContextMach_i386.h"

using namespace lldb;
using namespace lldb_private;

RegisterContextMach_i386::RegisterContextMach_i386(Thread &thread,
                                                   uint32_t concrete_frame_idx)
    : RegisterContextDarwin_i386(thread, concrete_frame_idx) {}

RegisterContextMach_i386::~RegisterContextMach_i386() {}

int RegisterContextMach_i386::DoReadGPR(lldb::tid_t tid, int flavor, GPR &gpr) {
  mach_msg_type_number_t count = GPRWordCount;
  return ::thread_get_state(tid, flavor, (thread_state_t)&gpr, &count);
}

int RegisterContextMach_i386::DoReadFPU(lldb::tid_t tid, int flavor, FPU &fpu) {
  mach_msg_type_number_t count = FPUWordCount;
  return ::thread_get_state(tid, flavor, (thread_state_t)&fpu, &count);
}

int RegisterContextMach_i386::DoReadEXC(lldb::tid_t tid, int flavor, EXC &exc) {
  mach_msg_type_number_t count = EXCWordCount;
  return ::thread_get_state(tid, flavor, (thread_state_t)&exc, &count);
}

int RegisterContextMach_i386::DoWriteGPR(lldb::tid_t tid, int flavor,
                                         const GPR &gpr) {
  return ::thread_set_state(
      tid, flavor, reinterpret_cast<thread_state_t>(const_cast<GPR *>(&gpr)),
      GPRWordCount);
}

int RegisterContextMach_i386::DoWriteFPU(lldb::tid_t tid, int flavor,
                                         const FPU &fpu) {
  return ::thread_set_state(
      tid, flavor, reinterpret_cast<thread_state_t>(const_cast<FPU *>(&fpu)),
      FPUWordCount);
}

int RegisterContextMach_i386::DoWriteEXC(lldb::tid_t tid, int flavor,
                                         const EXC &exc) {
  return ::thread_set_state(
      tid, flavor, reinterpret_cast<thread_state_t>(const_cast<EXC *>(&exc)),
      EXCWordCount);
}

#endif
