//===-- HostInfoWindows.cpp -------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Host/windows/windows.h"

#include <objbase.h>

#include <mutex> // std::once

#include "lldb/Host/windows/HostInfoWindows.h"
#include "lldb/Host/windows/PosixApi.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/ConvertUTF.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Threading.h"
#include "llvm/Support/raw_ostream.h"

using namespace lldb_private;

FileSpec HostInfoWindows::m_program_filespec;

void HostInfoWindows::Initialize() {
  ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  HostInfoBase::Initialize();
}

void HostInfoWindows::Terminate() {
  HostInfoBase::Terminate();
  ::CoUninitialize();
}

size_t HostInfoWindows::GetPageSize() {
  SYSTEM_INFO systemInfo;
  GetNativeSystemInfo(&systemInfo);
  return systemInfo.dwPageSize;
}

llvm::VersionTuple HostInfoWindows::GetOSVersion() {
  OSVERSIONINFOEX info;

  ZeroMemory(&info, sizeof(OSVERSIONINFOEX));
  info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
#pragma warning(push)
#pragma warning(disable : 4996)
  // Starting with Microsoft SDK for Windows 8.1, this function is deprecated
  // in favor of the new Windows Version Helper APIs.  Since we don't specify a
  // minimum SDK version, it's easier to simply disable the warning rather than
  // try to support both APIs.
  if (GetVersionEx((LPOSVERSIONINFO)&info) == 0)
    return llvm::VersionTuple();
#pragma warning(pop)

  return llvm::VersionTuple(info.dwMajorVersion, info.dwMinorVersion,
                            info.wServicePackMajor);
}

bool HostInfoWindows::GetOSBuildString(std::string &s) {
  s.clear();
  llvm::VersionTuple version = GetOSVersion();
  if (version.empty())
    return false;

  llvm::raw_string_ostream stream(s);
  stream << "Windows NT " << version.getAsString();
  return true;
}

bool HostInfoWindows::GetOSKernelDescription(std::string &s) {
  return GetOSBuildString(s);
}

bool HostInfoWindows::GetHostname(std::string &s) {
  wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD dwSize = MAX_COMPUTERNAME_LENGTH + 1;
  if (!::GetComputerNameW(buffer, &dwSize))
    return false;

  return llvm::convertWideToUTF8(buffer, s);
}

FileSpec HostInfoWindows::GetProgramFileSpec() {
  static llvm::once_flag g_once_flag;
  llvm::call_once(g_once_flag, []() {
    std::vector<wchar_t> buffer(PATH_MAX);
    ::GetModuleFileNameW(NULL, buffer.data(), buffer.size());
    std::string path;
    llvm::convertWideToUTF8(buffer.data(), path);
    m_program_filespec.SetFile(path, false, FileSpec::Style::native);
  });
  return m_program_filespec;
}

FileSpec HostInfoWindows::GetDefaultShell() {
  // Try to retrieve ComSpec from the environment. On the rare occasion
  // that it fails, try a well-known path for ComSpec instead.

  std::string shell;
  if (GetEnvironmentVar("ComSpec", shell))
    return FileSpec(shell, false);

  return FileSpec("C:\\Windows\\system32\\cmd.exe", false);
}

bool HostInfoWindows::GetEnvironmentVar(const std::string &var_name,
                                        std::string &var) {
  std::wstring wvar_name;
  if (!llvm::ConvertUTF8toWide(var_name, wvar_name))
    return false;

  if (const wchar_t *wvar = _wgetenv(wvar_name.c_str()))
    return llvm::convertWideToUTF8(wvar, var);
  return false;
}
