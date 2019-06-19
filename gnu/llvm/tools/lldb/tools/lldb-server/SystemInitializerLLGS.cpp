//===-- SystemInitializerLLGS.cpp -------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "SystemInitializerLLGS.h"

#if defined(__APPLE__)
#include "Plugins/ObjectFile/Mach-O/ObjectFileMachO.h"
using HostObjectFile = ObjectFileMachO;
#elif defined(_WIN32)
#include "Plugins/ObjectFile/PECOFF/ObjectFilePECOFF.h"
using HostObjectFile = ObjectFilePECOFF;
#else
#include "Plugins/ObjectFile/ELF/ObjectFileELF.h"
using HostObjectFile = ObjectFileELF;
#endif

using namespace lldb_private;

void SystemInitializerLLGS::Initialize() {
  SystemInitializerCommon::Initialize();
  HostObjectFile::Initialize();
}

void SystemInitializerLLGS::Terminate() {
  HostObjectFile::Terminate();
  SystemInitializerCommon::Terminate();
}
