//===-- RegisterContextPOSIXCore_mips64.cpp ---------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "RegisterContextPOSIXCore_mips64.h"

#include "lldb/Core/RegisterValue.h"
#include "lldb/Target/Thread.h"

using namespace lldb_private;

RegisterContextCorePOSIX_mips64::RegisterContextCorePOSIX_mips64(
    Thread &thread, RegisterInfoInterface *register_info,
    const DataExtractor &gpregset, const DataExtractor &fpregset)
    : RegisterContextPOSIX_mips64(thread, 0, register_info) {
  m_gpr_buffer.reset(
      new DataBufferHeap(gpregset.GetDataStart(), gpregset.GetByteSize()));
  m_gpr.SetData(m_gpr_buffer);
  m_gpr.SetByteOrder(gpregset.GetByteOrder());
  m_fpr_buffer.reset(
      new DataBufferHeap(fpregset.GetDataStart(), fpregset.GetByteSize()));
  m_fpr.SetData(m_fpr_buffer);
  m_fpr.SetByteOrder(fpregset.GetByteOrder());
}

RegisterContextCorePOSIX_mips64::~RegisterContextCorePOSIX_mips64() {}

bool RegisterContextCorePOSIX_mips64::ReadGPR() { return true; }

bool RegisterContextCorePOSIX_mips64::ReadFPR() { return false; }

bool RegisterContextCorePOSIX_mips64::WriteGPR() {
  assert(0);
  return false;
}

bool RegisterContextCorePOSIX_mips64::WriteFPR() {
  assert(0);
  return false;
}

bool RegisterContextCorePOSIX_mips64::ReadRegister(const RegisterInfo *reg_info,
                                                   RegisterValue &value) {
  
  lldb::offset_t offset = reg_info->byte_offset;
  lldb_private::ArchSpec arch = m_register_info_ap->GetTargetArchitecture();
  uint64_t v;
  if (IsGPR(reg_info->kinds[lldb::eRegisterKindLLDB])) {
    if (reg_info->byte_size == 4 && !(arch.GetMachine() == llvm::Triple::mips64el))
      // In case of 32bit core file, the register data are placed at 4 byte
      // offset. 
      offset = offset / 2;
    v = m_gpr.GetMaxU64(&offset, reg_info->byte_size);
    value = v;
    return true;
  } else if (IsFPR(reg_info->kinds[lldb::eRegisterKindLLDB])) {
    offset = offset - sizeof(GPR_linux_mips);
    v =m_fpr.GetMaxU64(&offset, reg_info->byte_size);
    value = v;
    return true;
    }
  return false;
}

bool RegisterContextCorePOSIX_mips64::ReadAllRegisterValues(
    lldb::DataBufferSP &data_sp) {
  return false;
}

bool RegisterContextCorePOSIX_mips64::WriteRegister(
    const RegisterInfo *reg_info, const RegisterValue &value) {
  return false;
}

bool RegisterContextCorePOSIX_mips64::WriteAllRegisterValues(
    const lldb::DataBufferSP &data_sp) {
  return false;
}

bool RegisterContextCorePOSIX_mips64::HardwareSingleStep(bool enable) {
  return false;
}
