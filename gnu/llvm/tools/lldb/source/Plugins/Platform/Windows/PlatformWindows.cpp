//===-- PlatformWindows.cpp -------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "PlatformWindows.h"

// C Includes
#include <stdio.h>
#if defined(_WIN32)
#include "lldb/Host/windows/windows.h"
#include <winsock2.h>
#endif

// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Breakpoint/BreakpointLocation.h"
#include "lldb/Breakpoint/BreakpointSite.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/ModuleSpec.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/Status.h"

using namespace lldb;
using namespace lldb_private;

static uint32_t g_initialize_count = 0;

namespace {
class SupportedArchList {
public:
  SupportedArchList() {
    AddArch(ArchSpec("i686-pc-windows"));
    AddArch(HostInfo::GetArchitecture(HostInfo::eArchKindDefault));
    AddArch(HostInfo::GetArchitecture(HostInfo::eArchKind32));
    AddArch(HostInfo::GetArchitecture(HostInfo::eArchKind64));
    AddArch(ArchSpec("i386-pc-windows"));
  }

  size_t Count() const { return m_archs.size(); }

  const ArchSpec &operator[](int idx) { return m_archs[idx]; }

private:
  void AddArch(const ArchSpec &spec) {
    auto iter = std::find_if(
        m_archs.begin(), m_archs.end(),
        [spec](const ArchSpec &rhs) { return spec.IsExactMatch(rhs); });
    if (iter != m_archs.end())
      return;
    if (spec.IsValid())
      m_archs.push_back(spec);
  }

  std::vector<ArchSpec> m_archs;
};
} // anonymous namespace

PlatformSP PlatformWindows::CreateInstance(bool force,
                                           const lldb_private::ArchSpec *arch) {
  // The only time we create an instance is when we are creating a remote
  // windows platform
  const bool is_host = false;

  bool create = force;
  if (create == false && arch && arch->IsValid()) {
    const llvm::Triple &triple = arch->GetTriple();
    switch (triple.getVendor()) {
    case llvm::Triple::PC:
      create = true;
      break;

    case llvm::Triple::UnknownArch:
      create = !arch->TripleVendorWasSpecified();
      break;

    default:
      break;
    }

    if (create) {
      switch (triple.getOS()) {
      case llvm::Triple::Win32:
        break;

      case llvm::Triple::UnknownOS:
        create = arch->TripleOSWasSpecified();
        break;

      default:
        create = false;
        break;
      }
    }
  }
  if (create)
    return PlatformSP(new PlatformWindows(is_host));
  return PlatformSP();
}

lldb_private::ConstString PlatformWindows::GetPluginNameStatic(bool is_host) {
  if (is_host) {
    static ConstString g_host_name(Platform::GetHostPlatformName());
    return g_host_name;
  } else {
    static ConstString g_remote_name("remote-windows");
    return g_remote_name;
  }
}

const char *PlatformWindows::GetPluginDescriptionStatic(bool is_host) {
  return is_host ? "Local Windows user platform plug-in."
                 : "Remote Windows user platform plug-in.";
}

lldb_private::ConstString PlatformWindows::GetPluginName() {
  return GetPluginNameStatic(IsHost());
}

void PlatformWindows::Initialize() {
  Platform::Initialize();

  if (g_initialize_count++ == 0) {
#if defined(_WIN32)
    WSADATA dummy;
    WSAStartup(MAKEWORD(2, 2), &dummy);
    // Force a host flag to true for the default platform object.
    PlatformSP default_platform_sp(new PlatformWindows(true));
    default_platform_sp->SetSystemArchitecture(HostInfo::GetArchitecture());
    Platform::SetHostPlatform(default_platform_sp);
#endif
    PluginManager::RegisterPlugin(
        PlatformWindows::GetPluginNameStatic(false),
        PlatformWindows::GetPluginDescriptionStatic(false),
        PlatformWindows::CreateInstance);
  }
}

void PlatformWindows::Terminate(void) {
  if (g_initialize_count > 0) {
    if (--g_initialize_count == 0) {
#ifdef _WIN32
      WSACleanup();
#endif
      PluginManager::UnregisterPlugin(PlatformWindows::CreateInstance);
    }
  }

  Platform::Terminate();
}

//------------------------------------------------------------------
/// Default Constructor
//------------------------------------------------------------------
PlatformWindows::PlatformWindows(bool is_host) : Platform(is_host) {}

//------------------------------------------------------------------
/// Destructor.
///
/// The destructor is virtual since this class is designed to be
/// inherited from by the plug-in instance.
//------------------------------------------------------------------
PlatformWindows::~PlatformWindows() = default;

bool PlatformWindows::GetModuleSpec(const FileSpec &module_file_spec,
                                    const ArchSpec &arch,
                                    ModuleSpec &module_spec) {
  if (m_remote_platform_sp)
    return m_remote_platform_sp->GetModuleSpec(module_file_spec, arch,
                                               module_spec);

  return Platform::GetModuleSpec(module_file_spec, arch, module_spec);
}

Status PlatformWindows::ResolveExecutable(
    const ModuleSpec &ms, lldb::ModuleSP &exe_module_sp,
    const FileSpecList *module_search_paths_ptr) {
  Status error;
  // Nothing special to do here, just use the actual file and architecture

  char exe_path[PATH_MAX];
  ModuleSpec resolved_module_spec(ms);

  if (IsHost()) {
    // if we cant resolve the executable loation based on the current path
    // variables
    if (!resolved_module_spec.GetFileSpec().Exists()) {
      resolved_module_spec.GetFileSpec().GetPath(exe_path, sizeof(exe_path));
      resolved_module_spec.GetFileSpec().SetFile(exe_path, true);
    }

    if (!resolved_module_spec.GetFileSpec().Exists())
      resolved_module_spec.GetFileSpec().ResolveExecutableLocation();

    if (resolved_module_spec.GetFileSpec().Exists())
      error.Clear();
    else {
      ms.GetFileSpec().GetPath(exe_path, sizeof(exe_path));
      error.SetErrorStringWithFormat("unable to find executable for '%s'",
                                     exe_path);
    }
  } else {
    if (m_remote_platform_sp) {
      error = GetCachedExecutable(resolved_module_spec, exe_module_sp, nullptr,
                                  *m_remote_platform_sp);
    } else {
      // We may connect to a process and use the provided executable (Don't use
      // local $PATH).
      if (resolved_module_spec.GetFileSpec().Exists())
        error.Clear();
      else
        error.SetErrorStringWithFormat("the platform is not currently "
                                       "connected, and '%s' doesn't exist in "
                                       "the system root.",
                                       exe_path);
    }
  }

  if (error.Success()) {
    if (resolved_module_spec.GetArchitecture().IsValid()) {
      error = ModuleList::GetSharedModule(resolved_module_spec, exe_module_sp,
                                          nullptr, nullptr, nullptr);

      if (!exe_module_sp || exe_module_sp->GetObjectFile() == nullptr) {
        exe_module_sp.reset();
        error.SetErrorStringWithFormat(
            "'%s' doesn't contain the architecture %s",
            resolved_module_spec.GetFileSpec().GetPath().c_str(),
            resolved_module_spec.GetArchitecture().GetArchitectureName());
      }
    } else {
      // No valid architecture was specified, ask the platform for
      // the architectures that we should be using (in the correct order)
      // and see if we can find a match that way
      StreamString arch_names;
      for (uint32_t idx = 0; GetSupportedArchitectureAtIndex(
               idx, resolved_module_spec.GetArchitecture());
           ++idx) {
        error = ModuleList::GetSharedModule(resolved_module_spec, exe_module_sp,
                                            nullptr, nullptr, nullptr);
        // Did we find an executable using one of the
        if (error.Success()) {
          if (exe_module_sp && exe_module_sp->GetObjectFile())
            break;
          else
            error.SetErrorToGenericError();
        }

        if (idx > 0)
          arch_names.PutCString(", ");
        arch_names.PutCString(
            resolved_module_spec.GetArchitecture().GetArchitectureName());
      }

      if (error.Fail() || !exe_module_sp) {
        if (resolved_module_spec.GetFileSpec().Readable()) {
          error.SetErrorStringWithFormat(
              "'%s' doesn't contain any '%s' platform architectures: %s",
              resolved_module_spec.GetFileSpec().GetPath().c_str(),
              GetPluginName().GetCString(), arch_names.GetData());
        } else {
          error.SetErrorStringWithFormat(
              "'%s' is not readable",
              resolved_module_spec.GetFileSpec().GetPath().c_str());
        }
      }
    }
  }

  return error;
}

bool PlatformWindows::GetRemoteOSVersion() {
  if (m_remote_platform_sp)
    return m_remote_platform_sp->GetOSVersion(
        m_major_os_version, m_minor_os_version, m_update_os_version);
  return false;
}

bool PlatformWindows::GetRemoteOSBuildString(std::string &s) {
  if (m_remote_platform_sp)
    return m_remote_platform_sp->GetRemoteOSBuildString(s);
  s.clear();
  return false;
}

bool PlatformWindows::GetRemoteOSKernelDescription(std::string &s) {
  if (m_remote_platform_sp)
    return m_remote_platform_sp->GetRemoteOSKernelDescription(s);
  s.clear();
  return false;
}

// Remote Platform subclasses need to override this function
ArchSpec PlatformWindows::GetRemoteSystemArchitecture() {
  if (m_remote_platform_sp)
    return m_remote_platform_sp->GetRemoteSystemArchitecture();
  return ArchSpec();
}

const char *PlatformWindows::GetHostname() {
  if (IsHost())
    return Platform::GetHostname();

  if (m_remote_platform_sp)
    return m_remote_platform_sp->GetHostname();
  return nullptr;
}

bool PlatformWindows::IsConnected() const {
  if (IsHost())
    return true;
  else if (m_remote_platform_sp)
    return m_remote_platform_sp->IsConnected();
  return false;
}

Status PlatformWindows::ConnectRemote(Args &args) {
  Status error;
  if (IsHost()) {
    error.SetErrorStringWithFormat(
        "can't connect to the host platform '%s', always connected",
        GetPluginName().AsCString());
  } else {
    if (!m_remote_platform_sp)
      m_remote_platform_sp =
          Platform::Create(ConstString("remote-gdb-server"), error);

    if (m_remote_platform_sp) {
      if (error.Success()) {
        if (m_remote_platform_sp) {
          error = m_remote_platform_sp->ConnectRemote(args);
        } else {
          error.SetErrorString(
              "\"platform connect\" takes a single argument: <connect-url>");
        }
      }
    } else
      error.SetErrorString("failed to create a 'remote-gdb-server' platform");

    if (error.Fail())
      m_remote_platform_sp.reset();
  }

  return error;
}

Status PlatformWindows::DisconnectRemote() {
  Status error;

  if (IsHost()) {
    error.SetErrorStringWithFormat(
        "can't disconnect from the host platform '%s', always connected",
        GetPluginName().AsCString());
  } else {
    if (m_remote_platform_sp)
      error = m_remote_platform_sp->DisconnectRemote();
    else
      error.SetErrorString("the platform is not currently connected");
  }
  return error;
}

bool PlatformWindows::GetProcessInfo(lldb::pid_t pid,
                                     ProcessInstanceInfo &process_info) {
  bool success = false;
  if (IsHost()) {
    success = Platform::GetProcessInfo(pid, process_info);
  } else if (m_remote_platform_sp) {
    success = m_remote_platform_sp->GetProcessInfo(pid, process_info);
  }
  return success;
}

uint32_t
PlatformWindows::FindProcesses(const ProcessInstanceInfoMatch &match_info,
                               ProcessInstanceInfoList &process_infos) {
  uint32_t match_count = 0;
  if (IsHost()) {
    // Let the base class figure out the host details
    match_count = Platform::FindProcesses(match_info, process_infos);
  } else {
    // If we are remote, we can only return results if we are connected
    if (m_remote_platform_sp)
      match_count =
          m_remote_platform_sp->FindProcesses(match_info, process_infos);
  }
  return match_count;
}

Status PlatformWindows::LaunchProcess(ProcessLaunchInfo &launch_info) {
  Status error;
  if (IsHost()) {
    error = Platform::LaunchProcess(launch_info);
  } else {
    if (m_remote_platform_sp)
      error = m_remote_platform_sp->LaunchProcess(launch_info);
    else
      error.SetErrorString("the platform is not currently connected");
  }
  return error;
}

ProcessSP PlatformWindows::DebugProcess(ProcessLaunchInfo &launch_info,
                                        Debugger &debugger, Target *target,
                                        Status &error) {
  // Windows has special considerations that must be followed when launching or
  // attaching to a process.  The
  // key requirement is that when launching or attaching to a process, you must
  // do it from the same the thread
  // that will go into a permanent loop which will then receive debug events
  // from the process.  In particular,
  // this means we can't use any of LLDB's generic mechanisms to do it for us,
  // because it doesn't have the
  // special knowledge required for setting up the background thread or passing
  // the right flags.
  //
  // Another problem is that that LLDB's standard model for debugging a process
  // is to first launch it, have
  // it stop at the entry point, and then attach to it.  In Windows this doesn't
  // quite work, you have to
  // specify as an argument to CreateProcess() that you're going to debug the
  // process.  So we override DebugProcess
  // here to handle this.  Launch operations go directly to the process plugin,
  // and attach operations almost go
  // directly to the process plugin (but we hijack the events first).  In
  // essence, we encapsulate all the logic
  // of Launching and Attaching in the process plugin, and
  // PlatformWindows::DebugProcess is just a pass-through
  // to get to the process plugin.

  if (launch_info.GetProcessID() != LLDB_INVALID_PROCESS_ID) {
    // This is a process attach.  Don't need to launch anything.
    ProcessAttachInfo attach_info(launch_info);
    return Attach(attach_info, debugger, target, error);
  } else {
    ProcessSP process_sp =
        target->CreateProcess(launch_info.GetListenerForProcess(debugger),
                              launch_info.GetProcessPluginName(), nullptr);

    // We need to launch and attach to the process.
    launch_info.GetFlags().Set(eLaunchFlagDebug);
    if (process_sp)
      error = process_sp->Launch(launch_info);

    return process_sp;
  }
}

lldb::ProcessSP PlatformWindows::Attach(ProcessAttachInfo &attach_info,
                                        Debugger &debugger, Target *target,
                                        Status &error) {
  error.Clear();
  lldb::ProcessSP process_sp;
  if (!IsHost()) {
    if (m_remote_platform_sp)
      process_sp =
          m_remote_platform_sp->Attach(attach_info, debugger, target, error);
    else
      error.SetErrorString("the platform is not currently connected");
    return process_sp;
  }

  if (target == nullptr) {
    TargetSP new_target_sp;
    FileSpec emptyFileSpec;
    ArchSpec emptyArchSpec;

    error = debugger.GetTargetList().CreateTarget(debugger, "", "", false,
                                                  nullptr, new_target_sp);
    target = new_target_sp.get();
  }

  if (!target || error.Fail())
    return process_sp;

  debugger.GetTargetList().SetSelectedTarget(target);

  const char *plugin_name = attach_info.GetProcessPluginName();
  process_sp = target->CreateProcess(
      attach_info.GetListenerForProcess(debugger), plugin_name, nullptr);

  process_sp->HijackProcessEvents(attach_info.GetHijackListener());
  if (process_sp)
    error = process_sp->Attach(attach_info);

  return process_sp;
}

const char *PlatformWindows::GetUserName(uint32_t uid) {
  // Check the cache in Platform in case we have already looked this uid up
  const char *user_name = Platform::GetUserName(uid);
  if (user_name)
    return user_name;

  if (IsRemote() && m_remote_platform_sp)
    return m_remote_platform_sp->GetUserName(uid);
  return nullptr;
}

const char *PlatformWindows::GetGroupName(uint32_t gid) {
  const char *group_name = Platform::GetGroupName(gid);
  if (group_name)
    return group_name;

  if (IsRemote() && m_remote_platform_sp)
    return m_remote_platform_sp->GetGroupName(gid);
  return nullptr;
}

Status PlatformWindows::GetFileWithUUID(const FileSpec &platform_file,
                                        const UUID *uuid_ptr,
                                        FileSpec &local_file) {
  if (IsRemote()) {
    if (m_remote_platform_sp)
      return m_remote_platform_sp->GetFileWithUUID(platform_file, uuid_ptr,
                                                   local_file);
  }

  // Default to the local case
  local_file = platform_file;
  return Status();
}

Status PlatformWindows::GetSharedModule(
    const ModuleSpec &module_spec, Process *process, ModuleSP &module_sp,
    const FileSpecList *module_search_paths_ptr, ModuleSP *old_module_sp_ptr,
    bool *did_create_ptr) {
  Status error;
  module_sp.reset();

  if (IsRemote()) {
    // If we have a remote platform always, let it try and locate
    // the shared module first.
    if (m_remote_platform_sp) {
      error = m_remote_platform_sp->GetSharedModule(
          module_spec, process, module_sp, module_search_paths_ptr,
          old_module_sp_ptr, did_create_ptr);
    }
  }

  if (!module_sp) {
    // Fall back to the local platform and find the file locally
    error = Platform::GetSharedModule(module_spec, process, module_sp,
                                      module_search_paths_ptr,
                                      old_module_sp_ptr, did_create_ptr);
  }
  if (module_sp)
    module_sp->SetPlatformFileSpec(module_spec.GetFileSpec());
  return error;
}

bool PlatformWindows::GetSupportedArchitectureAtIndex(uint32_t idx,
                                                      ArchSpec &arch) {
  static SupportedArchList architectures;

  if (idx >= architectures.Count())
    return false;
  arch = architectures[idx];
  return true;
}

void PlatformWindows::GetStatus(Stream &strm) {
  Platform::GetStatus(strm);

#ifdef _WIN32
  uint32_t major;
  uint32_t minor;
  uint32_t update;
  if (!HostInfo::GetOSVersion(major, minor, update)) {
    strm << "Windows";
    return;
  }

  strm << "Host: Windows " << major << '.' << minor << " Build: " << update
       << '\n';
#endif
}

bool PlatformWindows::CanDebugProcess() { return true; }

size_t PlatformWindows::GetEnvironment(StringList &env) {
  if (IsRemote()) {
    if (m_remote_platform_sp)
      return m_remote_platform_sp->GetEnvironment(env);
    return 0;
  }

  return Host::GetEnvironment(env);
}

ConstString PlatformWindows::GetFullNameForDylib(ConstString basename) {
  if (basename.IsEmpty())
    return basename;

  StreamString stream;
  stream.Printf("%s.dll", basename.GetCString());
  return ConstString(stream.GetString());
}
