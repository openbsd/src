//===-- OptionValueUUID.cpp ------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Interpreter/OptionValueUUID.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Core/Module.h"
#include "lldb/Interpreter/CommandInterpreter.h"
#include "lldb/Utility/Stream.h"
#include "lldb/Utility/StringList.h"

using namespace lldb;
using namespace lldb_private;

void OptionValueUUID::DumpValue(const ExecutionContext *exe_ctx, Stream &strm,
                                uint32_t dump_mask) {
  if (dump_mask & eDumpOptionType)
    strm.Printf("(%s)", GetTypeAsCString());
  if (dump_mask & eDumpOptionValue) {
    if (dump_mask & eDumpOptionType)
      strm.PutCString(" = ");
    m_uuid.Dump(&strm);
  }
}

Status OptionValueUUID::SetValueFromString(llvm::StringRef value,
                                           VarSetOperationType op) {
  Status error;
  switch (op) {
  case eVarSetOperationClear:
    Clear();
    NotifyValueChanged();
    break;

  case eVarSetOperationReplace:
  case eVarSetOperationAssign: {
    if (m_uuid.SetFromCString(value.str().c_str()) == 0)
      error.SetErrorStringWithFormat("invalid uuid string value '%s'",
                                     value.str().c_str());
    else {
      m_value_was_set = true;
      NotifyValueChanged();
    }
  } break;

  case eVarSetOperationInsertBefore:
  case eVarSetOperationInsertAfter:
  case eVarSetOperationRemove:
  case eVarSetOperationAppend:
  case eVarSetOperationInvalid:
    error = OptionValue::SetValueFromString(value, op);
    break;
  }
  return error;
}

lldb::OptionValueSP OptionValueUUID::DeepCopy() const {
  return OptionValueSP(new OptionValueUUID(*this));
}

size_t OptionValueUUID::AutoComplete(CommandInterpreter &interpreter,
                                     llvm::StringRef s, int match_start_point,
                                     int max_return_elements,
                                     bool &word_complete, StringList &matches) {
  word_complete = false;
  matches.Clear();
  ExecutionContext exe_ctx(interpreter.GetExecutionContext());
  Target *target = exe_ctx.GetTargetPtr();
  if (target) {
    const size_t num_modules = target->GetImages().GetSize();
    if (num_modules > 0) {
      UUID::ValueType uuid_bytes;
      uint32_t num_bytes_decoded = 0;
      UUID::DecodeUUIDBytesFromString(s, uuid_bytes, num_bytes_decoded);
      for (size_t i = 0; i < num_modules; ++i) {
        ModuleSP module_sp(target->GetImages().GetModuleAtIndex(i));
        if (module_sp) {
          const UUID &module_uuid = module_sp->GetUUID();
          if (module_uuid.IsValid()) {
            bool add_uuid = false;
            if (num_bytes_decoded == 0)
              add_uuid = true;
            else
              add_uuid = ::memcmp(module_uuid.GetBytes(), uuid_bytes,
                                  num_bytes_decoded) == 0;
            if (add_uuid) {
              std::string uuid_str;
              uuid_str = module_uuid.GetAsString();
              if (!uuid_str.empty())
                matches.AppendString(uuid_str.c_str());
            }
          }
        }
      }
    }
  }
  return matches.GetSize();
}
