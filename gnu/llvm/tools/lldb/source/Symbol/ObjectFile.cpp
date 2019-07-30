//===-- ObjectFile.cpp ------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Symbol/ObjectFile.h"
#include "Plugins/ObjectContainer/BSD-Archive/ObjectContainerBSDArchive.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/ModuleSpec.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/Section.h"
#include "lldb/Symbol/ObjectContainer.h"
#include "lldb/Symbol/SymbolFile.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/SectionLoadList.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/DataBuffer.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataBufferLLVM.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/RegularExpression.h"
#include "lldb/Utility/Timer.h"
#include "lldb/lldb-private.h"

using namespace lldb;
using namespace lldb_private;

ObjectFileSP
ObjectFile::FindPlugin(const lldb::ModuleSP &module_sp, const FileSpec *file,
                       lldb::offset_t file_offset, lldb::offset_t file_size,
                       DataBufferSP &data_sp, lldb::offset_t &data_offset) {
  ObjectFileSP object_file_sp;

  if (module_sp) {
    static Timer::Category func_cat(LLVM_PRETTY_FUNCTION);
    Timer scoped_timer(
        func_cat,
        "ObjectFile::FindPlugin (module = %s, file = %p, file_offset = "
        "0x%8.8" PRIx64 ", file_size = 0x%8.8" PRIx64 ")",
        module_sp->GetFileSpec().GetPath().c_str(),
        static_cast<const void *>(file), static_cast<uint64_t>(file_offset),
        static_cast<uint64_t>(file_size));
    if (file) {
      FileSpec archive_file;
      ObjectContainerCreateInstance create_object_container_callback;

      const bool file_exists = file->Exists();
      if (!data_sp) {
        // We have an object name which most likely means we have
        // a .o file in a static archive (.a file). Try and see if
        // we have a cached archive first without reading any data
        // first
        if (file_exists && module_sp->GetObjectName()) {
          for (uint32_t idx = 0;
               (create_object_container_callback =
                    PluginManager::GetObjectContainerCreateCallbackAtIndex(
                        idx)) != nullptr;
               ++idx) {
            std::unique_ptr<ObjectContainer> object_container_ap(
                create_object_container_callback(module_sp, data_sp,
                                                 data_offset, file, file_offset,
                                                 file_size));

            if (object_container_ap.get())
              object_file_sp = object_container_ap->GetObjectFile(file);

            if (object_file_sp.get())
              return object_file_sp;
          }
        }
        // Ok, we didn't find any containers that have a named object, now
        // lets read the first 512 bytes from the file so the object file
        // and object container plug-ins can use these bytes to see if they
        // can parse this file.
        if (file_size > 0) {
          data_sp =
              DataBufferLLVM::CreateSliceFromPath(file->GetPath(), 512, file_offset);
          data_offset = 0;
        }
      }

      if (!data_sp || data_sp->GetByteSize() == 0) {
        // Check for archive file with format "/path/to/archive.a(object.o)"
        char path_with_object[PATH_MAX * 2];
        module_sp->GetFileSpec().GetPath(path_with_object,
                                         sizeof(path_with_object));

        ConstString archive_object;
        const bool must_exist = true;
        if (ObjectFile::SplitArchivePathWithObject(
                path_with_object, archive_file, archive_object, must_exist)) {
          file_size = archive_file.GetByteSize();
          if (file_size > 0) {
            file = &archive_file;
            module_sp->SetFileSpecAndObjectName(archive_file, archive_object);
            // Check if this is a object container by iterating through all
            // object
            // container plugin instances and then trying to get an object file
            // from the container plugins since we had a name. Also, don't read
            // ANY data in case there is data cached in the container plug-ins
            // (like BSD archives caching the contained objects within an file).
            for (uint32_t idx = 0;
                 (create_object_container_callback =
                      PluginManager::GetObjectContainerCreateCallbackAtIndex(
                          idx)) != nullptr;
                 ++idx) {
              std::unique_ptr<ObjectContainer> object_container_ap(
                  create_object_container_callback(module_sp, data_sp,
                                                   data_offset, file,
                                                   file_offset, file_size));

              if (object_container_ap.get())
                object_file_sp = object_container_ap->GetObjectFile(file);

              if (object_file_sp.get())
                return object_file_sp;
            }
            // We failed to find any cached object files in the container
            // plug-ins, so lets read the first 512 bytes and try again below...
            data_sp = DataBufferLLVM::CreateSliceFromPath(archive_file.GetPath(),
                                                     512, file_offset);
          }
        }
      }

      if (data_sp && data_sp->GetByteSize() > 0) {
        // Check if this is a normal object file by iterating through
        // all object file plugin instances.
        ObjectFileCreateInstance create_object_file_callback;
        for (uint32_t idx = 0;
             (create_object_file_callback =
                  PluginManager::GetObjectFileCreateCallbackAtIndex(idx)) !=
             nullptr;
             ++idx) {
          object_file_sp.reset(create_object_file_callback(
              module_sp, data_sp, data_offset, file, file_offset, file_size));
          if (object_file_sp.get())
            return object_file_sp;
        }

        // Check if this is a object container by iterating through
        // all object container plugin instances and then trying to get
        // an object file from the container.
        for (uint32_t idx = 0;
             (create_object_container_callback =
                  PluginManager::GetObjectContainerCreateCallbackAtIndex(
                      idx)) != nullptr;
             ++idx) {
          std::unique_ptr<ObjectContainer> object_container_ap(
              create_object_container_callback(module_sp, data_sp, data_offset,
                                               file, file_offset, file_size));

          if (object_container_ap.get())
            object_file_sp = object_container_ap->GetObjectFile(file);

          if (object_file_sp.get())
            return object_file_sp;
        }
      }
    }
  }
  // We didn't find it, so clear our shared pointer in case it
  // contains anything and return an empty shared pointer
  object_file_sp.reset();
  return object_file_sp;
}

ObjectFileSP ObjectFile::FindPlugin(const lldb::ModuleSP &module_sp,
                                    const ProcessSP &process_sp,
                                    lldb::addr_t header_addr,
                                    DataBufferSP &data_sp) {
  ObjectFileSP object_file_sp;

  if (module_sp) {
    static Timer::Category func_cat(LLVM_PRETTY_FUNCTION);
    Timer scoped_timer(func_cat,
                       "ObjectFile::FindPlugin (module = "
                       "%s, process = %p, header_addr = "
                       "0x%" PRIx64 ")",
                       module_sp->GetFileSpec().GetPath().c_str(),
                       static_cast<void *>(process_sp.get()), header_addr);
    uint32_t idx;

    // Check if this is a normal object file by iterating through
    // all object file plugin instances.
    ObjectFileCreateMemoryInstance create_callback;
    for (idx = 0;
         (create_callback =
              PluginManager::GetObjectFileCreateMemoryCallbackAtIndex(idx)) !=
         nullptr;
         ++idx) {
      object_file_sp.reset(
          create_callback(module_sp, data_sp, process_sp, header_addr));
      if (object_file_sp.get())
        return object_file_sp;
    }
  }

  // We didn't find it, so clear our shared pointer in case it
  // contains anything and return an empty shared pointer
  object_file_sp.reset();
  return object_file_sp;
}

size_t ObjectFile::GetModuleSpecifications(const FileSpec &file,
                                           lldb::offset_t file_offset,
                                           lldb::offset_t file_size,
                                           ModuleSpecList &specs) {
  DataBufferSP data_sp = DataBufferLLVM::CreateSliceFromPath(file.GetPath(), 512, file_offset);
  if (data_sp) {
    if (file_size == 0) {
      const lldb::offset_t actual_file_size = file.GetByteSize();
      if (actual_file_size > file_offset)
        file_size = actual_file_size - file_offset;
    }
    return ObjectFile::GetModuleSpecifications(file,        // file spec
                                               data_sp,     // data bytes
                                               0,           // data offset
                                               file_offset, // file offset
                                               file_size,   // file length
                                               specs);
  }
  return 0;
}

size_t ObjectFile::GetModuleSpecifications(
    const lldb_private::FileSpec &file, lldb::DataBufferSP &data_sp,
    lldb::offset_t data_offset, lldb::offset_t file_offset,
    lldb::offset_t file_size, lldb_private::ModuleSpecList &specs) {
  const size_t initial_count = specs.GetSize();
  ObjectFileGetModuleSpecifications callback;
  uint32_t i;
  // Try the ObjectFile plug-ins
  for (i = 0;
       (callback =
            PluginManager::GetObjectFileGetModuleSpecificationsCallbackAtIndex(
                i)) != nullptr;
       ++i) {
    if (callback(file, data_sp, data_offset, file_offset, file_size, specs) > 0)
      return specs.GetSize() - initial_count;
  }

  // Try the ObjectContainer plug-ins
  for (i = 0;
       (callback = PluginManager::
            GetObjectContainerGetModuleSpecificationsCallbackAtIndex(i)) !=
       nullptr;
       ++i) {
    if (callback(file, data_sp, data_offset, file_offset, file_size, specs) > 0)
      return specs.GetSize() - initial_count;
  }
  return 0;
}

ObjectFile::ObjectFile(const lldb::ModuleSP &module_sp,
                       const FileSpec *file_spec_ptr,
                       lldb::offset_t file_offset, lldb::offset_t length,
                       const lldb::DataBufferSP &data_sp,
                       lldb::offset_t data_offset)
    : ModuleChild(module_sp),
      m_file(), // This file could be different from the original module's file
      m_type(eTypeInvalid), m_strata(eStrataInvalid),
      m_file_offset(file_offset), m_length(length), m_data(),
      m_unwind_table(*this), m_process_wp(),
      m_memory_addr(LLDB_INVALID_ADDRESS), m_sections_ap(), m_symtab_ap(),
      m_synthetic_symbol_idx(0) {
  if (file_spec_ptr)
    m_file = *file_spec_ptr;
  if (data_sp)
    m_data.SetData(data_sp, data_offset, length);
  Log *log(lldb_private::GetLogIfAllCategoriesSet(LIBLLDB_LOG_OBJECT));
  if (log)
    log->Printf("%p ObjectFile::ObjectFile() module = %p (%s), file = %s, "
                "file_offset = 0x%8.8" PRIx64 ", size = %" PRIu64,
                static_cast<void *>(this), static_cast<void *>(module_sp.get()),
                module_sp->GetSpecificationDescription().c_str(),
                m_file ? m_file.GetPath().c_str() : "<NULL>", m_file_offset,
                m_length);
}

ObjectFile::ObjectFile(const lldb::ModuleSP &module_sp,
                       const ProcessSP &process_sp, lldb::addr_t header_addr,
                       DataBufferSP &header_data_sp)
    : ModuleChild(module_sp), m_file(), m_type(eTypeInvalid),
      m_strata(eStrataInvalid), m_file_offset(0), m_length(0), m_data(),
      m_unwind_table(*this), m_process_wp(process_sp),
      m_memory_addr(header_addr), m_sections_ap(), m_symtab_ap(),
      m_synthetic_symbol_idx(0) {
  if (header_data_sp)
    m_data.SetData(header_data_sp, 0, header_data_sp->GetByteSize());
  Log *log(lldb_private::GetLogIfAllCategoriesSet(LIBLLDB_LOG_OBJECT));
  if (log)
    log->Printf("%p ObjectFile::ObjectFile() module = %p (%s), process = %p, "
                "header_addr = 0x%" PRIx64,
                static_cast<void *>(this), static_cast<void *>(module_sp.get()),
                module_sp->GetSpecificationDescription().c_str(),
                static_cast<void *>(process_sp.get()), m_memory_addr);
}

ObjectFile::~ObjectFile() {
  Log *log(lldb_private::GetLogIfAllCategoriesSet(LIBLLDB_LOG_OBJECT));
  if (log)
    log->Printf("%p ObjectFile::~ObjectFile ()\n", static_cast<void *>(this));
}

bool ObjectFile::SetModulesArchitecture(const ArchSpec &new_arch) {
  ModuleSP module_sp(GetModule());
  if (module_sp)
    return module_sp->SetArchitecture(new_arch);
  return false;
}

AddressClass ObjectFile::GetAddressClass(addr_t file_addr) {
  Symtab *symtab = GetSymtab();
  if (symtab) {
    Symbol *symbol = symtab->FindSymbolContainingFileAddress(file_addr);
    if (symbol) {
      if (symbol->ValueIsAddress()) {
        const SectionSP section_sp(symbol->GetAddressRef().GetSection());
        if (section_sp) {
          const SectionType section_type = section_sp->GetType();
          switch (section_type) {
          case eSectionTypeInvalid:
            return eAddressClassUnknown;
          case eSectionTypeCode:
            return eAddressClassCode;
          case eSectionTypeContainer:
            return eAddressClassUnknown;
          case eSectionTypeData:
          case eSectionTypeDataCString:
          case eSectionTypeDataCStringPointers:
          case eSectionTypeDataSymbolAddress:
          case eSectionTypeData4:
          case eSectionTypeData8:
          case eSectionTypeData16:
          case eSectionTypeDataPointers:
          case eSectionTypeZeroFill:
          case eSectionTypeDataObjCMessageRefs:
          case eSectionTypeDataObjCCFStrings:
          case eSectionTypeGoSymtab:
            return eAddressClassData;
          case eSectionTypeDebug:
          case eSectionTypeDWARFDebugAbbrev:
          case eSectionTypeDWARFDebugAddr:
          case eSectionTypeDWARFDebugAranges:
          case eSectionTypeDWARFDebugFrame:
          case eSectionTypeDWARFDebugInfo:
          case eSectionTypeDWARFDebugLine:
          case eSectionTypeDWARFDebugLoc:
          case eSectionTypeDWARFDebugMacInfo:
          case eSectionTypeDWARFDebugMacro:
          case eSectionTypeDWARFDebugPubNames:
          case eSectionTypeDWARFDebugPubTypes:
          case eSectionTypeDWARFDebugRanges:
          case eSectionTypeDWARFDebugStr:
          case eSectionTypeDWARFDebugStrOffsets:
          case eSectionTypeDWARFAppleNames:
          case eSectionTypeDWARFAppleTypes:
          case eSectionTypeDWARFAppleNamespaces:
          case eSectionTypeDWARFAppleObjC:
            return eAddressClassDebug;
          case eSectionTypeEHFrame:
          case eSectionTypeARMexidx:
          case eSectionTypeARMextab:
          case eSectionTypeCompactUnwind:
            return eAddressClassRuntime;
          case eSectionTypeELFSymbolTable:
          case eSectionTypeELFDynamicSymbols:
          case eSectionTypeELFRelocationEntries:
          case eSectionTypeELFDynamicLinkInfo:
          case eSectionTypeOther:
            return eAddressClassUnknown;
          case eSectionTypeAbsoluteAddress:
            // In case of absolute sections decide the address class based on
            // the symbol
            // type because the section type isn't specify if it is a code or a
            // data
            // section.
            break;
          }
        }
      }

      const SymbolType symbol_type = symbol->GetType();
      switch (symbol_type) {
      case eSymbolTypeAny:
        return eAddressClassUnknown;
      case eSymbolTypeAbsolute:
        return eAddressClassUnknown;
      case eSymbolTypeCode:
        return eAddressClassCode;
      case eSymbolTypeTrampoline:
        return eAddressClassCode;
      case eSymbolTypeResolver:
        return eAddressClassCode;
      case eSymbolTypeData:
        return eAddressClassData;
      case eSymbolTypeRuntime:
        return eAddressClassRuntime;
      case eSymbolTypeException:
        return eAddressClassRuntime;
      case eSymbolTypeSourceFile:
        return eAddressClassDebug;
      case eSymbolTypeHeaderFile:
        return eAddressClassDebug;
      case eSymbolTypeObjectFile:
        return eAddressClassDebug;
      case eSymbolTypeCommonBlock:
        return eAddressClassDebug;
      case eSymbolTypeBlock:
        return eAddressClassDebug;
      case eSymbolTypeLocal:
        return eAddressClassData;
      case eSymbolTypeParam:
        return eAddressClassData;
      case eSymbolTypeVariable:
        return eAddressClassData;
      case eSymbolTypeVariableType:
        return eAddressClassDebug;
      case eSymbolTypeLineEntry:
        return eAddressClassDebug;
      case eSymbolTypeLineHeader:
        return eAddressClassDebug;
      case eSymbolTypeScopeBegin:
        return eAddressClassDebug;
      case eSymbolTypeScopeEnd:
        return eAddressClassDebug;
      case eSymbolTypeAdditional:
        return eAddressClassUnknown;
      case eSymbolTypeCompiler:
        return eAddressClassDebug;
      case eSymbolTypeInstrumentation:
        return eAddressClassDebug;
      case eSymbolTypeUndefined:
        return eAddressClassUnknown;
      case eSymbolTypeObjCClass:
        return eAddressClassRuntime;
      case eSymbolTypeObjCMetaClass:
        return eAddressClassRuntime;
      case eSymbolTypeObjCIVar:
        return eAddressClassRuntime;
      case eSymbolTypeReExported:
        return eAddressClassRuntime;
      }
    }
  }
  return eAddressClassUnknown;
}

DataBufferSP ObjectFile::ReadMemory(const ProcessSP &process_sp,
                                    lldb::addr_t addr, size_t byte_size) {
  DataBufferSP data_sp;
  if (process_sp) {
    std::unique_ptr<DataBufferHeap> data_ap(new DataBufferHeap(byte_size, 0));
    Status error;
    const size_t bytes_read = process_sp->ReadMemory(
        addr, data_ap->GetBytes(), data_ap->GetByteSize(), error);
    if (bytes_read == byte_size)
      data_sp.reset(data_ap.release());
  }
  return data_sp;
}

size_t ObjectFile::GetData(lldb::offset_t offset, size_t length,
                           DataExtractor &data) const {
  // The entire file has already been mmap'ed into m_data, so just copy from
  // there
  // as the back mmap buffer will be shared with shared pointers.
  return data.SetData(m_data, offset, length);
}

size_t ObjectFile::CopyData(lldb::offset_t offset, size_t length,
                            void *dst) const {
  // The entire file has already been mmap'ed into m_data, so just copy from
  // there
  // Note that the data remains in target byte order.
  return m_data.CopyData(offset, length, dst);
}

size_t ObjectFile::ReadSectionData(const Section *section,
                                   lldb::offset_t section_offset, void *dst,
                                   size_t dst_len) const {
  assert(section);
  section_offset *= section->GetTargetByteSize();

  // If some other objectfile owns this data, pass this to them.
  if (section->GetObjectFile() != this)
    return section->GetObjectFile()->ReadSectionData(section, section_offset,
                                                     dst, dst_len);

  if (IsInMemory()) {
    ProcessSP process_sp(m_process_wp.lock());
    if (process_sp) {
      Status error;
      const addr_t base_load_addr =
          section->GetLoadBaseAddress(&process_sp->GetTarget());
      if (base_load_addr != LLDB_INVALID_ADDRESS)
        return process_sp->ReadMemory(base_load_addr + section_offset, dst,
                                      dst_len, error);
    }
  } else {
    const lldb::offset_t section_file_size = section->GetFileSize();
    if (section_offset < section_file_size) {
      const size_t section_bytes_left = section_file_size - section_offset;
      size_t section_dst_len = dst_len;
      if (section_dst_len > section_bytes_left)
        section_dst_len = section_bytes_left;
      return CopyData(section->GetFileOffset() + section_offset,
                      section_dst_len, dst);
    } else {
      if (section->GetType() == eSectionTypeZeroFill) {
        const uint64_t section_size = section->GetByteSize();
        const uint64_t section_bytes_left = section_size - section_offset;
        uint64_t section_dst_len = dst_len;
        if (section_dst_len > section_bytes_left)
          section_dst_len = section_bytes_left;
        memset(dst, 0, section_dst_len);
        return section_dst_len;
      }
    }
  }
  return 0;
}

//----------------------------------------------------------------------
// Get the section data the file on disk
//----------------------------------------------------------------------
size_t ObjectFile::ReadSectionData(const Section *section,
                                   DataExtractor &section_data) const {
  // If some other objectfile owns this data, pass this to them.
  if (section->GetObjectFile() != this)
    return section->GetObjectFile()->ReadSectionData(section, section_data);

  if (IsInMemory()) {
    ProcessSP process_sp(m_process_wp.lock());
    if (process_sp) {
      const addr_t base_load_addr =
          section->GetLoadBaseAddress(&process_sp->GetTarget());
      if (base_load_addr != LLDB_INVALID_ADDRESS) {
        DataBufferSP data_sp(
            ReadMemory(process_sp, base_load_addr, section->GetByteSize()));
        if (data_sp) {
          section_data.SetData(data_sp, 0, data_sp->GetByteSize());
          section_data.SetByteOrder(process_sp->GetByteOrder());
          section_data.SetAddressByteSize(process_sp->GetAddressByteSize());
          return section_data.GetByteSize();
        }
      }
    }
    return GetData(section->GetFileOffset(), section->GetFileSize(),
                   section_data);
  } else {
    // The object file now contains a full mmap'ed copy of the object file data,
    // so just use this
    return MemoryMapSectionData(section, section_data);
  }
}

size_t ObjectFile::MemoryMapSectionData(const Section *section,
                                        DataExtractor &section_data) const {
  // If some other objectfile owns this data, pass this to them.
  if (section->GetObjectFile() != this)
    return section->GetObjectFile()->MemoryMapSectionData(section,
                                                          section_data);

  if (IsInMemory()) {
    return ReadSectionData(section, section_data);
  } else {
    // The object file now contains a full mmap'ed copy of the object file data,
    // so just use this
    return GetData(section->GetFileOffset(), section->GetFileSize(),
                   section_data);
  }
}

bool ObjectFile::SplitArchivePathWithObject(const char *path_with_object,
                                            FileSpec &archive_file,
                                            ConstString &archive_object,
                                            bool must_exist) {
  RegularExpression g_object_regex(llvm::StringRef("(.*)\\(([^\\)]+)\\)$"));
  RegularExpression::Match regex_match(2);
  if (g_object_regex.Execute(llvm::StringRef::withNullAsEmpty(path_with_object),
                             &regex_match)) {
    std::string path;
    std::string obj;
    if (regex_match.GetMatchAtIndex(path_with_object, 1, path) &&
        regex_match.GetMatchAtIndex(path_with_object, 2, obj)) {
      archive_file.SetFile(path, false);
      archive_object.SetCString(obj.c_str());
      if (must_exist && !archive_file.Exists())
        return false;
      return true;
    }
  }
  return false;
}

void ObjectFile::ClearSymtab() {
  ModuleSP module_sp(GetModule());
  if (module_sp) {
    std::lock_guard<std::recursive_mutex> guard(module_sp->GetMutex());
    Log *log(lldb_private::GetLogIfAllCategoriesSet(LIBLLDB_LOG_OBJECT));
    if (log)
      log->Printf("%p ObjectFile::ClearSymtab () symtab = %p",
                  static_cast<void *>(this),
                  static_cast<void *>(m_symtab_ap.get()));
    m_symtab_ap.reset();
  }
}

SectionList *ObjectFile::GetSectionList(bool update_module_section_list) {
  if (m_sections_ap.get() == nullptr) {
    if (update_module_section_list) {
      ModuleSP module_sp(GetModule());
      if (module_sp) {
        std::lock_guard<std::recursive_mutex> guard(module_sp->GetMutex());
        CreateSections(*module_sp->GetUnifiedSectionList());
      }
    } else {
      SectionList unified_section_list;
      CreateSections(unified_section_list);
    }
  }
  return m_sections_ap.get();
}

lldb::SymbolType
ObjectFile::GetSymbolTypeFromName(llvm::StringRef name,
                                  lldb::SymbolType symbol_type_hint) {
  if (!name.empty()) {
    if (name.startswith("_OBJC_")) {
      // ObjC
      if (name.startswith("_OBJC_CLASS_$_"))
        return lldb::eSymbolTypeObjCClass;
      if (name.startswith("_OBJC_METACLASS_$_"))
        return lldb::eSymbolTypeObjCMetaClass;
      if (name.startswith("_OBJC_IVAR_$_"))
        return lldb::eSymbolTypeObjCIVar;
    } else if (name.startswith(".objc_class_name_")) {
      // ObjC v1
      return lldb::eSymbolTypeObjCClass;
    }
  }
  return symbol_type_hint;
}

ConstString ObjectFile::GetNextSyntheticSymbolName() {
  StreamString ss;
  ConstString file_name = GetModule()->GetFileSpec().GetFilename();
  ss.Printf("___lldb_unnamed_symbol%u$$%s", ++m_synthetic_symbol_idx,
            file_name.GetCString());
  return ConstString(ss.GetString());
}

Status ObjectFile::LoadInMemory(Target &target, bool set_pc) {
  Status error;
  ProcessSP process = target.CalculateProcess();
  if (!process)
    return Status("No Process");
  if (set_pc && !GetEntryPointAddress().IsValid())
    return Status("No entry address in object file");

  SectionList *section_list = GetSectionList();
  if (!section_list)
    return Status("No section in object file");
  size_t section_count = section_list->GetNumSections(0);
  for (size_t i = 0; i < section_count; ++i) {
    SectionSP section_sp = section_list->GetSectionAtIndex(i);
    addr_t addr = target.GetSectionLoadList().GetSectionLoadAddress(section_sp);
    if (addr != LLDB_INVALID_ADDRESS) {
      DataExtractor section_data;
      // We can skip sections like bss
      if (section_sp->GetFileSize() == 0)
        continue;
      section_sp->GetSectionData(section_data);
      lldb::offset_t written = process->WriteMemory(
          addr, section_data.GetDataStart(), section_data.GetByteSize(), error);
      if (written != section_data.GetByteSize())
        return error;
    }
  }
  if (set_pc) {
    ThreadList &thread_list = process->GetThreadList();
    ThreadSP curr_thread(thread_list.GetSelectedThread());
    RegisterContextSP reg_context(curr_thread->GetRegisterContext());
    Address file_entry = GetEntryPointAddress();
    reg_context->SetPC(file_entry.GetLoadAddress(&target));
  }
  return error;
}
