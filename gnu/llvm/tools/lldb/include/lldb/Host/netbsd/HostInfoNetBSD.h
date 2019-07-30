//===-- HostInfoNetBSD.h ---------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef lldb_Host_netbsd_HostInfoNetBSD_h_
#define lldb_Host_netbsd_HostInfoNetBSD_h_

#include "lldb/Host/posix/HostInfoPosix.h"
#include "lldb/Utility/FileSpec.h"

namespace lldb_private {

class HostInfoNetBSD : public HostInfoPosix {
public:
  static bool GetOSVersion(uint32_t &major, uint32_t &minor, uint32_t &update);
  static bool GetOSBuildString(std::string &s);
  static bool GetOSKernelDescription(std::string &s);
  static FileSpec GetProgramFileSpec();
};
}

#endif
