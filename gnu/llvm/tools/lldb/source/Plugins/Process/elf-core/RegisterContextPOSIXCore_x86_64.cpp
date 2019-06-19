//===-- RegisterContextPOSIXCore_x86_64.cpp ---------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "RegisterContextPOSIXCore_x86_64.h"
#include "lldb/Core/RegisterValue.h"
#include "lldb/Target/Thread.h"
#include "lldb/Utility/DataExtractor.h"

using namespace lldb_private;

RegisterContextCorePOSIX_x86_64::RegisterContextCorePOSIX_x86_64(
    Thread &thread, RegisterInfoInterface *register_info,
    const DataExtractor &gpregset, llvm::ArrayRef<CoreNote> notes)
    : RegisterContextPOSIX_x86(thread, 0, register_info) {
  size_t size, len;

  size = GetGPRSize();
  m_gpregset.reset(new uint8_t[size]);
  len =
      gpregset.ExtractBytes(0, size, lldb::eByteOrderLittle, m_gpregset.get());
  if (len != size)
    m_gpregset.reset();

  DataExtractor fpregset = getRegset(
      notes, register_info->GetTargetArchitecture().GetTriple(), FPR_Desc);
  size = sizeof(FXSAVE);
  m_fpregset.reset(new uint8_t[size]);
  len =
      fpregset.ExtractBytes(0, size, lldb::eByteOrderLittle, m_fpregset.get());
  if (len != size)
    m_fpregset.reset();
}

bool RegisterContextCorePOSIX_x86_64::ReadGPR() {
  return m_gpregset != nullptr;
}

bool RegisterContextCorePOSIX_x86_64::ReadFPR() {
  return m_fpregset != nullptr;
}

bool RegisterContextCorePOSIX_x86_64::WriteGPR() {
  assert(0);
  return false;
}

bool RegisterContextCorePOSIX_x86_64::WriteFPR() {
  assert(0);
  return false;
}

bool RegisterContextCorePOSIX_x86_64::ReadRegister(const RegisterInfo *reg_info,
                                                   RegisterValue &value) {
  const uint8_t *src;
  size_t offset;
  const size_t fxsave_offset = reg_info->byte_offset - GetFXSAVEOffset();
  // make the offset relative to the beginning of the FXSAVE structure because
  // this is the data that we have (not the entire UserArea)

  if (m_gpregset && reg_info->byte_offset < GetGPRSize()) {
    src = m_gpregset.get();
    offset = reg_info->byte_offset;
  } else if (m_fpregset && fxsave_offset < sizeof(FXSAVE)) {
    src = m_fpregset.get();
    offset = fxsave_offset;
  } else {
    return false;
  }

  Status error;
  value.SetFromMemoryData(reg_info, src + offset, reg_info->byte_size,
                          lldb::eByteOrderLittle, error);

  return error.Success();
}

bool RegisterContextCorePOSIX_x86_64::ReadAllRegisterValues(
    lldb::DataBufferSP &data_sp) {
  return false;
}

bool RegisterContextCorePOSIX_x86_64::WriteRegister(
    const RegisterInfo *reg_info, const RegisterValue &value) {
  return false;
}

bool RegisterContextCorePOSIX_x86_64::WriteAllRegisterValues(
    const lldb::DataBufferSP &data_sp) {
  return false;
}

bool RegisterContextCorePOSIX_x86_64::HardwareSingleStep(bool enable) {
  return false;
}
