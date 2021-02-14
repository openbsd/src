//===-- NativeRegisterContextOpenBSD_arch.cpp ---------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// NativeRegisterContextOpenBSD_* contains the implementations for each
// supported architecture, and includes the static initalizer
// CreateHostNativeRegisterContextOpenBSD() implementation which returns a arch
// specific register context. In order to facilitate compiling lldb
// on architectures which do not have an RegisterContext implementation
// this file will include the relevant backend, and otherwise will
// include a stub implentation which just reports an error and exits.

#if defined(__arm64__) || defined(__aarch64__)
#include "NativeRegisterContextOpenBSD_arm64.cpp"
#elif defined(__x86_64__)
#include "NativeRegisterContextOpenBSD_x86_64.cpp"
#else

#include "Plugins/Process/OpenBSD/NativeRegisterContextOpenBSD.h"

using namespace lldb_private;
using namespace lldb_private::process_openbsd;

std::unique_ptr<NativeRegisterContextOpenBSD>
NativeRegisterContextOpenBSD::CreateHostNativeRegisterContextOpenBSD(
        const ArchSpec &target_arch, NativeThreadProtocol &native_thread) {
  return std::unique_ptr<NativeRegisterContextOpenBSD>{};
}

#endif
