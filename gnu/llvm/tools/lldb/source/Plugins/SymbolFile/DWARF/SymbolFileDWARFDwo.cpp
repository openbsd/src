//===-- SymbolFileDWARFDwo.cpp ----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "SymbolFileDWARFDwo.h"

#include "lldb/Core/Section.h"
#include "lldb/Expression/DWARFExpression.h"
#include "lldb/Symbol/ObjectFile.h"
#include "lldb/Utility/LLDBAssert.h"

#include "DWARFUnit.h"
#include "DWARFDebugInfo.h"

using namespace lldb;
using namespace lldb_private;

SymbolFileDWARFDwo::SymbolFileDWARFDwo(ObjectFileSP objfile,
                                       DWARFUnit *dwarf_cu)
    : SymbolFileDWARF(objfile.get()), m_obj_file_sp(objfile),
      m_base_dwarf_cu(dwarf_cu) {
  SetID(((lldb::user_id_t)dwarf_cu->GetOffset()) << 32);
}

void SymbolFileDWARFDwo::LoadSectionData(lldb::SectionType sect_type,
                                         DWARFDataExtractor &data) {
  const SectionList *section_list =
      m_obj_file->GetSectionList(false /* update_module_section_list */);
  if (section_list) {
    SectionSP section_sp(section_list->FindSectionByType(sect_type, true));
    if (section_sp) {
      // See if we memory mapped the DWARF segment?
      if (m_dwarf_data.GetByteSize()) {
        data.SetData(m_dwarf_data, section_sp->GetOffset(),
                     section_sp->GetFileSize());
        return;
      }

      if (m_obj_file->ReadSectionData(section_sp.get(), data) != 0)
        return;

      data.Clear();
    }
  }

  SymbolFileDWARF::LoadSectionData(sect_type, data);
}

lldb::CompUnitSP
SymbolFileDWARFDwo::ParseCompileUnit(DWARFUnit *dwarf_cu,
                                     uint32_t cu_idx) {
  assert(GetCompileUnit() == dwarf_cu && "SymbolFileDWARFDwo::ParseCompileUnit "
                                         "called with incompatible compile "
                                         "unit");
  return GetBaseSymbolFile()->ParseCompileUnit(m_base_dwarf_cu, UINT32_MAX);
}

DWARFUnit *SymbolFileDWARFDwo::GetCompileUnit() {
  // A clang module is found via a skeleton CU, but is not a proper DWO.
  // Clang modules have a .debug_info section instead of the *_dwo variant.
  if (auto *section_list = m_obj_file->GetSectionList(false))
    if (auto section_sp =
            section_list->FindSectionByType(eSectionTypeDWARFDebugInfo, true))
      if (!section_sp->GetName().GetStringRef().endswith("dwo"))
        return nullptr;

  // Only dwo files with 1 compile unit is supported
  if (GetNumCompileUnits() == 1)
    return DebugInfo()->GetCompileUnitAtIndex(0);
  else
    return nullptr;
}

DWARFUnit *
SymbolFileDWARFDwo::GetDWARFCompileUnit(lldb_private::CompileUnit *comp_unit) {
  return GetCompileUnit();
}

SymbolFileDWARF::DIEToTypePtr &SymbolFileDWARFDwo::GetDIEToType() {
  return GetBaseSymbolFile()->GetDIEToType();
}

SymbolFileDWARF::DIEToVariableSP &SymbolFileDWARFDwo::GetDIEToVariable() {
  return GetBaseSymbolFile()->GetDIEToVariable();
}

SymbolFileDWARF::DIEToClangType &
SymbolFileDWARFDwo::GetForwardDeclDieToClangType() {
  return GetBaseSymbolFile()->GetForwardDeclDieToClangType();
}

SymbolFileDWARF::ClangTypeToDIE &
SymbolFileDWARFDwo::GetForwardDeclClangTypeToDie() {
  return GetBaseSymbolFile()->GetForwardDeclClangTypeToDie();
}

size_t SymbolFileDWARFDwo::GetObjCMethodDIEOffsets(
    lldb_private::ConstString class_name, DIEArray &method_die_offsets) {
  return GetBaseSymbolFile()->GetObjCMethodDIEOffsets(
      class_name, method_die_offsets);
}

UniqueDWARFASTTypeMap &SymbolFileDWARFDwo::GetUniqueDWARFASTTypeMap() {
  return GetBaseSymbolFile()->GetUniqueDWARFASTTypeMap();
}

lldb::TypeSP SymbolFileDWARFDwo::FindDefinitionTypeForDWARFDeclContext(
    const DWARFDeclContext &die_decl_ctx) {
  return GetBaseSymbolFile()->FindDefinitionTypeForDWARFDeclContext(
      die_decl_ctx);
}

lldb::TypeSP SymbolFileDWARFDwo::FindCompleteObjCDefinitionTypeForDIE(
    const DWARFDIE &die, const lldb_private::ConstString &type_name,
    bool must_be_implementation) {
  return GetBaseSymbolFile()->FindCompleteObjCDefinitionTypeForDIE(
      die, type_name, must_be_implementation);
}

DWARFUnit *SymbolFileDWARFDwo::GetBaseCompileUnit() {
  return m_base_dwarf_cu;
}

SymbolFileDWARF *SymbolFileDWARFDwo::GetBaseSymbolFile() {
  return m_base_dwarf_cu->GetSymbolFileDWARF();
}

DWARFExpression::LocationListFormat
SymbolFileDWARFDwo::GetLocationListFormat() const {
  return DWARFExpression::SplitDwarfLocationList;
}

TypeSystem *
SymbolFileDWARFDwo::GetTypeSystemForLanguage(LanguageType language) {
  return GetBaseSymbolFile()->GetTypeSystemForLanguage(language);
}

DWARFDIE
SymbolFileDWARFDwo::GetDIE(const DIERef &die_ref) {
  lldbassert(m_base_dwarf_cu->GetOffset() == die_ref.cu_offset);
  return DebugInfo()->GetDIEForDIEOffset(die_ref.die_offset);
}
