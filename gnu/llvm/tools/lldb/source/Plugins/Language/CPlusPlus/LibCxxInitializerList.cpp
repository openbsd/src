//===-- LibCxxInitializerList.cpp -------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "LibCxx.h"

#include "lldb/Core/ValueObject.h"
#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Utility/ConstString.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace lldb_private {
namespace formatters {
class LibcxxInitializerListSyntheticFrontEnd
    : public SyntheticChildrenFrontEnd {
public:
  LibcxxInitializerListSyntheticFrontEnd(lldb::ValueObjectSP valobj_sp);

  ~LibcxxInitializerListSyntheticFrontEnd() override;

  size_t CalculateNumChildren() override;

  lldb::ValueObjectSP GetChildAtIndex(size_t idx) override;

  bool Update() override;

  bool MightHaveChildren() override;

  size_t GetIndexOfChildWithName(const ConstString &name) override;

private:
  ValueObject *m_start;
  CompilerType m_element_type;
  uint32_t m_element_size;
  size_t m_num_elements;
};
} // namespace formatters
} // namespace lldb_private

lldb_private::formatters::LibcxxInitializerListSyntheticFrontEnd::
    LibcxxInitializerListSyntheticFrontEnd(lldb::ValueObjectSP valobj_sp)
    : SyntheticChildrenFrontEnd(*valobj_sp), m_start(nullptr), m_element_type(),
      m_element_size(0), m_num_elements(0) {
  if (valobj_sp)
    Update();
}

lldb_private::formatters::LibcxxInitializerListSyntheticFrontEnd::
    ~LibcxxInitializerListSyntheticFrontEnd() {
  // this needs to stay around because it's a child object who will follow its
  // parent's life cycle
  // delete m_start;
}

size_t lldb_private::formatters::LibcxxInitializerListSyntheticFrontEnd::
    CalculateNumChildren() {
  static ConstString g___size_("__size_");
  m_num_elements = 0;
  ValueObjectSP size_sp(m_backend.GetChildMemberWithName(g___size_, true));
  if (size_sp)
    m_num_elements = size_sp->GetValueAsUnsigned(0);
  return m_num_elements;
}

lldb::ValueObjectSP lldb_private::formatters::
    LibcxxInitializerListSyntheticFrontEnd::GetChildAtIndex(size_t idx) {
  if (!m_start)
    return lldb::ValueObjectSP();

  uint64_t offset = idx * m_element_size;
  offset = offset + m_start->GetValueAsUnsigned(0);
  StreamString name;
  name.Printf("[%" PRIu64 "]", (uint64_t)idx);
  return CreateValueObjectFromAddress(name.GetString(), offset,
                                      m_backend.GetExecutionContextRef(),
                                      m_element_type);
}

bool lldb_private::formatters::LibcxxInitializerListSyntheticFrontEnd::
    Update() {
  static ConstString g___begin_("__begin_");

  m_start = nullptr;
  m_num_elements = 0;
  lldb::TemplateArgumentKind kind;
  m_element_type = m_backend.GetCompilerType().GetTemplateArgument(0, kind);
  if (kind != lldb::eTemplateArgumentKindType || !m_element_type.IsValid())
    return false;

  m_element_size = m_element_type.GetByteSize(nullptr);

  if (m_element_size > 0)
    m_start =
        m_backend.GetChildMemberWithName(g___begin_, true)
            .get(); // store raw pointers or end up with a circular dependency

  return false;
}

bool lldb_private::formatters::LibcxxInitializerListSyntheticFrontEnd::
    MightHaveChildren() {
  return true;
}

size_t lldb_private::formatters::LibcxxInitializerListSyntheticFrontEnd::
    GetIndexOfChildWithName(const ConstString &name) {
  if (!m_start)
    return UINT32_MAX;
  return ExtractIndexFromString(name.GetCString());
}

lldb_private::SyntheticChildrenFrontEnd *
lldb_private::formatters::LibcxxInitializerListSyntheticFrontEndCreator(
    CXXSyntheticChildren *, lldb::ValueObjectSP valobj_sp) {
  return (valobj_sp ? new LibcxxInitializerListSyntheticFrontEnd(valobj_sp)
                    : nullptr);
}
