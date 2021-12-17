//===-- PlatformOpenBSD.cpp -----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "PlatformOpenBSD.h"
#include "lldb/Host/Config.h"

#include <cstdio>
#if LLDB_ENABLE_POSIX
#include <sys/utsname.h>
#endif

#include "lldb/Core/Debugger.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/FileSpec.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/State.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/StreamString.h"

// Define these constants from OpenBSD mman.h for use when targeting remote
// openbsd systems even when host has different values.
#define MAP_PRIVATE 0x0002
#define MAP_ANON 0x1000

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::platform_openbsd;

LLDB_PLUGIN_DEFINE(PlatformOpenBSD)

static uint32_t g_initialize_count = 0;


PlatformSP PlatformOpenBSD::CreateInstance(bool force, const ArchSpec *arch) {
  Log *log(GetLogIfAllCategoriesSet(LIBLLDB_LOG_PLATFORM));
  LLDB_LOG(log, "force = {0}, arch=({1}, {2})", force,
           arch ? arch->GetArchitectureName() : "<null>",
           arch ? arch->GetTriple().getTriple() : "<null>");

  bool create = force;
  if (!create && arch && arch->IsValid()) {
    const llvm::Triple &triple = arch->GetTriple();
    switch (triple.getOS()) {
    case llvm::Triple::OpenBSD:
      create = true;
      break;

#if defined(__OpenBSD__)
    // Only accept "unknown" for the OS if the host is BSD and it "unknown"
    // wasn't specified (it was just returned because it was NOT specified)
    case llvm::Triple::OSType::UnknownOS:
      create = !arch->TripleOSWasSpecified();
      break;
#endif
    default:
      break;
    }
  }
  LLDB_LOG(log, "create = {0}", create);
  if (create) {
    return PlatformSP(new PlatformOpenBSD(false));
  }
  return PlatformSP();
}

ConstString PlatformOpenBSD::GetPluginNameStatic(bool is_host) {
  if (is_host) {
    static ConstString g_host_name(Platform::GetHostPlatformName());
    return g_host_name;
  } else {
    static ConstString g_remote_name("remote-openbsd");
    return g_remote_name;
  }
}

const char *PlatformOpenBSD::GetPluginDescriptionStatic(bool is_host) {
  if (is_host)
    return "Local OpenBSD user platform plug-in.";
  else
    return "Remote OpenBSD user platform plug-in.";
}

ConstString PlatformOpenBSD::GetPluginName() {
  return GetPluginNameStatic(IsHost());
}

void PlatformOpenBSD::Initialize() {
  PlatformPOSIX::Initialize();

  if (g_initialize_count++ == 0) {
#if defined(__OpenBSD__)
    PlatformSP default_platform_sp(new PlatformOpenBSD(true));
    default_platform_sp->SetSystemArchitecture(HostInfo::GetArchitecture());
    Platform::SetHostPlatform(default_platform_sp);
#endif
    PluginManager::RegisterPlugin(
        PlatformOpenBSD::GetPluginNameStatic(false),
        PlatformOpenBSD::GetPluginDescriptionStatic(false),
        PlatformOpenBSD::CreateInstance, nullptr);
  }
}

void PlatformOpenBSD::Terminate() {
  if (g_initialize_count > 0) {
    if (--g_initialize_count == 0) {
      PluginManager::UnregisterPlugin(PlatformOpenBSD::CreateInstance);
    }
  }

  PlatformPOSIX::Terminate();
}

/// Default Constructor
PlatformOpenBSD::PlatformOpenBSD(bool is_host)
    : PlatformPOSIX(is_host) // This is the local host platform
{}

bool PlatformOpenBSD::GetSupportedArchitectureAtIndex(uint32_t idx,
                                                      ArchSpec &arch) {
  if (IsHost()) {
    ArchSpec hostArch = HostInfo::GetArchitecture(HostInfo::eArchKindDefault);
    if (hostArch.GetTriple().isOSOpenBSD()) {
      if (idx == 0) {
        arch = hostArch;
        return arch.IsValid();
      }
    }
  } else {
    if (m_remote_platform_sp)
      return m_remote_platform_sp->GetSupportedArchitectureAtIndex(idx, arch);

    llvm::Triple triple;
    // Set the OS to OpenBSD
    triple.setOS(llvm::Triple::OpenBSD);
    // Set the architecture
    switch (idx) {
    case 0:
      triple.setArchName("x86_64");
      break;
    case 1:
      triple.setArchName("i386");
      break;
    case 2:
      triple.setArchName("aarch64");
      break;
    case 3:
      triple.setArchName("arm");
      break;
    default:
      return false;
    }
    // Leave the vendor as "llvm::Triple:UnknownVendor" and don't specify the
    // vendor by calling triple.SetVendorName("unknown") so that it is a
    // "unspecified unknown". This means when someone calls
    // triple.GetVendorName() it will return an empty string which indicates
    // that the vendor can be set when two architectures are merged

    // Now set the triple into "arch" and return true
    arch.SetTriple(triple);
    return true;
  }
  return false;
}

void PlatformOpenBSD::GetStatus(Stream &strm) {
  Platform::GetStatus(strm);

#if LLDB_ENABLE_POSIX
  // Display local kernel information only when we are running in host mode.
  // Otherwise, we would end up printing non-OpenBSD information (when running
  // on Mac OS for example).
  if (IsHost()) {
    struct utsname un;

    if (uname(&un))
      return;

    strm.Printf("    Kernel: %s\n", un.sysname);
    strm.Printf("   Release: %s\n", un.release);
    strm.Printf("   Version: %s\n", un.version);
  }
#endif
}

bool PlatformOpenBSD::CanDebugProcess() {
	if (IsHost()) {
		return true;
	} else {
		// If we're connected, we can debug.
		return IsConnected();
	}
}

// For local debugging, OpenBSD will override the debug logic to use llgs-launch
// rather than lldb-launch, llgs-attach.  This differs from current lldb-
// launch, debugserver-attach approach on MacOSX.
lldb::ProcessSP
PlatformOpenBSD::DebugProcess(ProcessLaunchInfo &launch_info, Debugger &debugger,
                             Target *target, // Can be NULL, if NULL create a new
                                             // target, else use existing one
                             Status &error) {
  Log *log(GetLogIfAllCategoriesSet(LIBLLDB_LOG_PLATFORM));
  LLDB_LOG(log, "target {0}", target);

  // If we're a remote host, use standard behavior from parent class.
  if (!IsHost())
    return PlatformPOSIX::DebugProcess(launch_info, debugger, target, error);

  //
  // For local debugging, we'll insist on having ProcessGDBRemote create the
  // process.
  //

  ProcessSP process_sp;

  // Make sure we stop at the entry point
  launch_info.GetFlags().Set(eLaunchFlagDebug);

  // We always launch the process we are going to debug in a separate process
  // group, since then we can handle ^C interrupts ourselves w/o having to
  // worry about the target getting them as well.
  launch_info.SetLaunchInSeparateProcessGroup(true);

  // Ensure we have a target.
  if (target == nullptr) {
    LLDB_LOG(log, "creating new target");
    TargetSP new_target_sp;
    error = debugger.GetTargetList().CreateTarget(debugger, "", "", eLoadDependentsNo,
                                                  nullptr, new_target_sp);
    if (error.Fail()) {
      LLDB_LOG(log, "failed to create new target: {0}", error);
      return process_sp;
    }

    target = new_target_sp.get();
    if (!target) {
      error.SetErrorString("CreateTarget() returned nullptr");
      LLDB_LOG(log, "error: {0}", error);
      return process_sp;
    }
  }

  // Mark target as currently selected target.
  //debugger.GetTargetList().SetSelectedTarget(target);

  // Now create the gdb-remote process.
  LLDB_LOG(log, "having target create process with gdb-remote plugin");
  process_sp = target->CreateProcess(
      launch_info.GetListener(), "gdb-remote", nullptr, true);

  if (!process_sp) {
    error.SetErrorString("CreateProcess() failed for gdb-remote process");
    LLDB_LOG(log, "error: {0}", error);
    return process_sp;
  }

  LLDB_LOG(log, "successfully created process");
  // Adjust launch for a hijacker.
  ListenerSP listener_sp;
  if (!launch_info.GetHijackListener()) {
    LLDB_LOG(log, "setting up hijacker");
    listener_sp =
        Listener::MakeListener("lldb.PlatformOpenBSD.DebugProcess.hijack");
    launch_info.SetHijackListener(listener_sp);
    process_sp->HijackProcessEvents(listener_sp);
  }

  // Log file actions.
  if (log) {
    LLDB_LOG(log, "launching process with the following file actions:");
    StreamString stream;
    size_t i = 0;
    const FileAction *file_action;
    while ((file_action = launch_info.GetFileActionAtIndex(i++)) != nullptr) {
      file_action->Dump(stream);
      LLDB_LOG(log, "{0}", stream.GetData());
      stream.Clear();
    }
  }

  // Do the launch.
  error = process_sp->Launch(launch_info);
  if (error.Success()) {
    // Handle the hijacking of process events.
    if (listener_sp) {
      const StateType state = process_sp->WaitForProcessToStop(
          llvm::None, NULL, false, listener_sp);

      LLDB_LOG(log, "pid {0} state {0}", process_sp->GetID(), state);
    }

    // Hook up process PTY if we have one (which we should for local debugging
    // with llgs).
    int pty_fd = launch_info.GetPTY().ReleasePrimaryFileDescriptor();
    if (pty_fd != PseudoTerminal::invalid_fd) {
      process_sp->SetSTDIOFileDescriptor(pty_fd);
      LLDB_LOG(log, "hooked up STDIO pty to process");
    } else
      LLDB_LOG(log, "not using process STDIO pty");
  } else {
    LLDB_LOG(log, "process launch failed: {0}", error);
    // FIXME figure out appropriate cleanup here.  Do we delete the target? Do
    // we delete the process?  Does our caller do that?
  }

  return process_sp;
}

void PlatformOpenBSD::CalculateTrapHandlerSymbolNames() {
  m_trap_handlers.push_back(ConstString("_sigtramp"));
}

MmapArgList PlatformOpenBSD::GetMmapArgumentList(const ArchSpec &arch,
                                                 addr_t addr, addr_t length,
                                                 unsigned prot, unsigned flags,
                                                 addr_t fd, addr_t offset) {
  uint64_t flags_platform = 0;

  if (flags & eMmapFlagsPrivate)
    flags_platform |= MAP_PRIVATE;
  if (flags & eMmapFlagsAnon)
    flags_platform |= MAP_ANON;

  MmapArgList args({addr, length, prot, flags_platform, fd, offset});
  return args;
}

FileSpec PlatformOpenBSD::LocateExecutable(const char *basename) {

  std::string check = std::string("/usr/bin/") + basename;
  if (access(check.c_str(), X_OK) == 0) {
    return FileSpec(check);
  }

  return FileSpec();
}
