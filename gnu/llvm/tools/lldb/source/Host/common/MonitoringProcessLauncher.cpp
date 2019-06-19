//===-- MonitoringProcessLauncher.cpp ---------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Host/MonitoringProcessLauncher.h"
#include "lldb/Host/HostProcess.h"
#include "lldb/Target/ProcessLaunchInfo.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/Status.h"

#include "llvm/Support/FileSystem.h"

using namespace lldb;
using namespace lldb_private;

MonitoringProcessLauncher::MonitoringProcessLauncher(
    std::unique_ptr<ProcessLauncher> delegate_launcher)
    : m_delegate_launcher(std::move(delegate_launcher)) {}

HostProcess
MonitoringProcessLauncher::LaunchProcess(const ProcessLaunchInfo &launch_info,
                                         Status &error) {
  ProcessLaunchInfo resolved_info(launch_info);

  error.Clear();

  FileSpec exe_spec(resolved_info.GetExecutableFile());

  llvm::sys::fs::file_status stats;
  status(exe_spec.GetPath(), stats);
  if (!exists(stats)) {
    exe_spec.ResolvePath();
    status(exe_spec.GetPath(), stats);
  }
  if (!exists(stats)) {
    exe_spec.ResolveExecutableLocation();
    status(exe_spec.GetPath(), stats);
  }

  if (!exists(stats)) {
    error.SetErrorStringWithFormatv("executable doesn't exist: '{0}'",
                                    exe_spec);
    return HostProcess();
  }

  resolved_info.SetExecutableFile(exe_spec, false);
  assert(!resolved_info.GetFlags().Test(eLaunchFlagLaunchInTTY));

  HostProcess process =
      m_delegate_launcher->LaunchProcess(resolved_info, error);

  if (process.GetProcessId() != LLDB_INVALID_PROCESS_ID) {
    Log *log(lldb_private::GetLogIfAllCategoriesSet(LIBLLDB_LOG_PROCESS));

    assert(launch_info.GetMonitorProcessCallback());
    process.StartMonitoring(launch_info.GetMonitorProcessCallback(),
                            launch_info.GetMonitorSignals());
    if (log)
      log->PutCString("started monitoring child process.");
  } else {
    // Invalid process ID, something didn't go well
    if (error.Success())
      error.SetErrorString("process launch failed for unknown reasons");
  }
  return process;
}
