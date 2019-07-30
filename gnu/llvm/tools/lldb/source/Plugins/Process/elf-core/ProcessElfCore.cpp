//===-- ProcessElfCore.cpp --------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// C Includes
#include <stdlib.h>

// C++ Includes
#include <mutex>

// Other libraries and framework includes
#include "lldb/Core/Module.h"
#include "lldb/Core/ModuleSpec.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/Section.h"
#include "lldb/Core/State.h"
#include "lldb/Target/DynamicLoader.h"
#include "lldb/Target/MemoryRegionInfo.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/UnixSignals.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/DataBufferLLVM.h"
#include "lldb/Utility/Log.h"

#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Threading.h"

#include "Plugins/DynamicLoader/POSIX-DYLD/DynamicLoaderPOSIXDYLD.h"
#include "Plugins/ObjectFile/ELF/ObjectFileELF.h"

// Project includes
#include "ProcessElfCore.h"
#include "ThreadElfCore.h"

using namespace lldb_private;

ConstString ProcessElfCore::GetPluginNameStatic() {
  static ConstString g_name("elf-core");
  return g_name;
}

const char *ProcessElfCore::GetPluginDescriptionStatic() {
  return "ELF core dump plug-in.";
}

void ProcessElfCore::Terminate() {
  PluginManager::UnregisterPlugin(ProcessElfCore::CreateInstance);
}

lldb::ProcessSP ProcessElfCore::CreateInstance(lldb::TargetSP target_sp,
                                               lldb::ListenerSP listener_sp,
                                               const FileSpec *crash_file) {
  lldb::ProcessSP process_sp;
  if (crash_file) {
    // Read enough data for a ELF32 header or ELF64 header
    // Note: Here we care about e_type field only, so it is safe
    // to ignore possible presence of the header extension.
    const size_t header_size = sizeof(llvm::ELF::Elf64_Ehdr);

    auto data_sp = DataBufferLLVM::CreateSliceFromPath(crash_file->GetPath(),
                                                       header_size, 0);
    if (data_sp && data_sp->GetByteSize() == header_size &&
        elf::ELFHeader::MagicBytesMatch(data_sp->GetBytes())) {
      elf::ELFHeader elf_header;
      DataExtractor data(data_sp, lldb::eByteOrderLittle, 4);
      lldb::offset_t data_offset = 0;
      if (elf_header.Parse(data, &data_offset)) {
        if (elf_header.e_type == llvm::ELF::ET_CORE)
          process_sp.reset(
              new ProcessElfCore(target_sp, listener_sp, *crash_file));
      }
    }
  }
  return process_sp;
}

bool ProcessElfCore::CanDebug(lldb::TargetSP target_sp,
                              bool plugin_specified_by_name) {
  // For now we are just making sure the file exists for a given module
  if (!m_core_module_sp && m_core_file.Exists()) {
    ModuleSpec core_module_spec(m_core_file, target_sp->GetArchitecture());
    Status error(ModuleList::GetSharedModule(core_module_spec, m_core_module_sp,
                                             NULL, NULL, NULL));
    if (m_core_module_sp) {
      ObjectFile *core_objfile = m_core_module_sp->GetObjectFile();
      if (core_objfile && core_objfile->GetType() == ObjectFile::eTypeCoreFile)
        return true;
    }
  }
  return false;
}

//----------------------------------------------------------------------
// ProcessElfCore constructor
//----------------------------------------------------------------------
ProcessElfCore::ProcessElfCore(lldb::TargetSP target_sp,
                               lldb::ListenerSP listener_sp,
                               const FileSpec &core_file)
    : Process(target_sp, listener_sp), m_core_module_sp(),
      m_core_file(core_file), m_dyld_plugin_name(),
      m_os(llvm::Triple::UnknownOS), m_thread_data_valid(false),
      m_thread_data(), m_core_aranges() {}

//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------
ProcessElfCore::~ProcessElfCore() {
  Clear();
  // We need to call finalize on the process before destroying ourselves
  // to make sure all of the broadcaster cleanup goes as planned. If we
  // destruct this class, then Process::~Process() might have problems
  // trying to fully destroy the broadcaster.
  Finalize();
}

//----------------------------------------------------------------------
// PluginInterface
//----------------------------------------------------------------------
ConstString ProcessElfCore::GetPluginName() { return GetPluginNameStatic(); }

uint32_t ProcessElfCore::GetPluginVersion() { return 1; }

lldb::addr_t ProcessElfCore::AddAddressRangeFromLoadSegment(
    const elf::ELFProgramHeader *header) {
  const lldb::addr_t addr = header->p_vaddr;
  FileRange file_range(header->p_offset, header->p_filesz);
  VMRangeToFileOffset::Entry range_entry(addr, header->p_memsz, file_range);

  VMRangeToFileOffset::Entry *last_entry = m_core_aranges.Back();
  if (last_entry && last_entry->GetRangeEnd() == range_entry.GetRangeBase() &&
      last_entry->data.GetRangeEnd() == range_entry.data.GetRangeBase() &&
      last_entry->GetByteSize() == last_entry->data.GetByteSize()) {
    last_entry->SetRangeEnd(range_entry.GetRangeEnd());
    last_entry->data.SetRangeEnd(range_entry.data.GetRangeEnd());
  } else {
    m_core_aranges.Append(range_entry);
  }

  // Keep a separate map of permissions that that isn't coalesced so all ranges
  // are maintained.
  const uint32_t permissions =
      ((header->p_flags & llvm::ELF::PF_R) ? lldb::ePermissionsReadable : 0u) |
      ((header->p_flags & llvm::ELF::PF_W) ? lldb::ePermissionsWritable : 0u) |
      ((header->p_flags & llvm::ELF::PF_X) ? lldb::ePermissionsExecutable : 0u);

  m_core_range_infos.Append(
      VMRangeToPermissions::Entry(addr, header->p_memsz, permissions));

  return addr;
}

//----------------------------------------------------------------------
// Process Control
//----------------------------------------------------------------------
Status ProcessElfCore::DoLoadCore() {
  Status error;
  if (!m_core_module_sp) {
    error.SetErrorString("invalid core module");
    return error;
  }

  ObjectFileELF *core = (ObjectFileELF *)(m_core_module_sp->GetObjectFile());
  if (core == NULL) {
    error.SetErrorString("invalid core object file");
    return error;
  }

  const uint32_t num_segments = core->GetProgramHeaderCount();
  if (num_segments == 0) {
    error.SetErrorString("core file has no segments");
    return error;
  }

  SetCanJIT(false);

  m_thread_data_valid = true;

  bool ranges_are_sorted = true;
  lldb::addr_t vm_addr = 0;
  /// Walk through segments and Thread and Address Map information.
  /// PT_NOTE - Contains Thread and Register information
  /// PT_LOAD - Contains a contiguous range of Process Address Space
  for (uint32_t i = 1; i <= num_segments; i++) {
    const elf::ELFProgramHeader *header = core->GetProgramHeaderByIndex(i);
    assert(header != NULL);

    DataExtractor data = core->GetSegmentDataByIndex(i);

    // Parse thread contexts and auxv structure
    if (header->p_type == llvm::ELF::PT_NOTE) {
      error = ParseThreadContextsFromNoteSegment(header, data);
      if (error.Fail())
        return error;
    }
    // PT_LOAD segments contains address map
    if (header->p_type == llvm::ELF::PT_LOAD) {
      lldb::addr_t last_addr = AddAddressRangeFromLoadSegment(header);
      if (vm_addr > last_addr)
        ranges_are_sorted = false;
      vm_addr = last_addr;
    }
  }

  if (!ranges_are_sorted) {
    m_core_aranges.Sort();
    m_core_range_infos.Sort();
  }

  // Even if the architecture is set in the target, we need to override
  // it to match the core file which is always single arch.
  ArchSpec arch(m_core_module_sp->GetArchitecture());

  ArchSpec target_arch = GetTarget().GetArchitecture();
  ArchSpec core_arch(m_core_module_sp->GetArchitecture());
  target_arch.MergeFrom(core_arch);
  GetTarget().SetArchitecture(target_arch);
 
  SetUnixSignals(UnixSignals::Create(GetArchitecture()));

  // Ensure we found at least one thread that was stopped on a signal.
  bool siginfo_signal_found = false;
  bool prstatus_signal_found = false;
  // Check we found a signal in a SIGINFO note.
  for (const auto &thread_data : m_thread_data) {
    if (thread_data.signo != 0)
      siginfo_signal_found = true;
    if (thread_data.prstatus_sig != 0)
      prstatus_signal_found = true;
  }
  if (!siginfo_signal_found) {
    // If we don't have signal from SIGINFO use the signal from each threads
    // PRSTATUS note.
    if (prstatus_signal_found) {
      for (auto &thread_data : m_thread_data)
        thread_data.signo = thread_data.prstatus_sig;
    } else if (m_thread_data.size() > 0) {
      // If all else fails force the first thread to be SIGSTOP
      m_thread_data.begin()->signo =
          GetUnixSignals()->GetSignalNumberFromName("SIGSTOP");
    }
  }

  // Core files are useless without the main executable. See if we can locate
  // the main
  // executable using data we found in the core file notes.
  lldb::ModuleSP exe_module_sp = GetTarget().GetExecutableModule();
  if (!exe_module_sp) {
    // The first entry in the NT_FILE might be our executable
    if (!m_nt_file_entries.empty()) {
      ModuleSpec exe_module_spec;
      exe_module_spec.GetArchitecture() = arch;
      exe_module_spec.GetFileSpec().SetFile(
          m_nt_file_entries[0].path.GetCString(), false);
      if (exe_module_spec.GetFileSpec()) {
        exe_module_sp = GetTarget().GetSharedModule(exe_module_spec);
        if (exe_module_sp)
          GetTarget().SetExecutableModule(exe_module_sp, false);
      }
    }
  }
  return error;
}

lldb_private::DynamicLoader *ProcessElfCore::GetDynamicLoader() {
  if (m_dyld_ap.get() == NULL)
    m_dyld_ap.reset(DynamicLoader::FindPlugin(
        this, DynamicLoaderPOSIXDYLD::GetPluginNameStatic().GetCString()));
  return m_dyld_ap.get();
}

bool ProcessElfCore::UpdateThreadList(ThreadList &old_thread_list,
                                      ThreadList &new_thread_list) {
  const uint32_t num_threads = GetNumThreadContexts();
  if (!m_thread_data_valid)
    return false;

  for (lldb::tid_t tid = 0; tid < num_threads; ++tid) {
    const ThreadData &td = m_thread_data[tid];
    lldb::ThreadSP thread_sp(new ThreadElfCore(*this, td));
    new_thread_list.AddThread(thread_sp);
  }
  return new_thread_list.GetSize(false) > 0;
}

void ProcessElfCore::RefreshStateAfterStop() {}

Status ProcessElfCore::DoDestroy() { return Status(); }

//------------------------------------------------------------------
// Process Queries
//------------------------------------------------------------------

bool ProcessElfCore::IsAlive() { return true; }

//------------------------------------------------------------------
// Process Memory
//------------------------------------------------------------------
size_t ProcessElfCore::ReadMemory(lldb::addr_t addr, void *buf, size_t size,
                                  Status &error) {
  // Don't allow the caching that lldb_private::Process::ReadMemory does
  // since in core files we have it all cached our our core file anyway.
  return DoReadMemory(addr, buf, size, error);
}

Status ProcessElfCore::GetMemoryRegionInfo(lldb::addr_t load_addr,
                                           MemoryRegionInfo &region_info) {
  region_info.Clear();
  const VMRangeToPermissions::Entry *permission_entry =
      m_core_range_infos.FindEntryThatContainsOrFollows(load_addr);
  if (permission_entry) {
    if (permission_entry->Contains(load_addr)) {
      region_info.GetRange().SetRangeBase(permission_entry->GetRangeBase());
      region_info.GetRange().SetRangeEnd(permission_entry->GetRangeEnd());
      const Flags permissions(permission_entry->data);
      region_info.SetReadable(permissions.Test(lldb::ePermissionsReadable)
                                  ? MemoryRegionInfo::eYes
                                  : MemoryRegionInfo::eNo);
      region_info.SetWritable(permissions.Test(lldb::ePermissionsWritable)
                                  ? MemoryRegionInfo::eYes
                                  : MemoryRegionInfo::eNo);
      region_info.SetExecutable(permissions.Test(lldb::ePermissionsExecutable)
                                    ? MemoryRegionInfo::eYes
                                    : MemoryRegionInfo::eNo);
      region_info.SetMapped(MemoryRegionInfo::eYes);
    } else if (load_addr < permission_entry->GetRangeBase()) {
      region_info.GetRange().SetRangeBase(load_addr);
      region_info.GetRange().SetRangeEnd(permission_entry->GetRangeBase());
      region_info.SetReadable(MemoryRegionInfo::eNo);
      region_info.SetWritable(MemoryRegionInfo::eNo);
      region_info.SetExecutable(MemoryRegionInfo::eNo);
      region_info.SetMapped(MemoryRegionInfo::eNo);
    }
    return Status();
  }

  region_info.GetRange().SetRangeBase(load_addr);
  region_info.GetRange().SetRangeEnd(LLDB_INVALID_ADDRESS);
  region_info.SetReadable(MemoryRegionInfo::eNo);
  region_info.SetWritable(MemoryRegionInfo::eNo);
  region_info.SetExecutable(MemoryRegionInfo::eNo);
  region_info.SetMapped(MemoryRegionInfo::eNo);
  return Status();
}

size_t ProcessElfCore::DoReadMemory(lldb::addr_t addr, void *buf, size_t size,
                                    Status &error) {
  ObjectFile *core_objfile = m_core_module_sp->GetObjectFile();

  if (core_objfile == NULL)
    return 0;

  // Get the address range
  const VMRangeToFileOffset::Entry *address_range =
      m_core_aranges.FindEntryThatContains(addr);
  if (address_range == NULL || address_range->GetRangeEnd() < addr) {
    error.SetErrorStringWithFormat("core file does not contain 0x%" PRIx64,
                                   addr);
    return 0;
  }

  // Convert the address into core file offset
  const lldb::addr_t offset = addr - address_range->GetRangeBase();
  const lldb::addr_t file_start = address_range->data.GetRangeBase();
  const lldb::addr_t file_end = address_range->data.GetRangeEnd();
  size_t bytes_to_read = size; // Number of bytes to read from the core file
  size_t bytes_copied = 0;   // Number of bytes actually read from the core file
  size_t zero_fill_size = 0; // Padding
  lldb::addr_t bytes_left =
      0; // Number of bytes available in the core file from the given address

  // Don't proceed if core file doesn't contain the actual data for this address range.
  if (file_start == file_end)
    return 0;

  // Figure out how many on-disk bytes remain in this segment
  // starting at the given offset
  if (file_end > file_start + offset)
    bytes_left = file_end - (file_start + offset);

  // Figure out how many bytes we need to zero-fill if we are
  // reading more bytes than available in the on-disk segment
  if (bytes_to_read > bytes_left) {
    zero_fill_size = bytes_to_read - bytes_left;
    bytes_to_read = bytes_left;
  }

  // If there is data available on the core file read it
  if (bytes_to_read)
    bytes_copied =
        core_objfile->CopyData(offset + file_start, bytes_to_read, buf);

  assert(zero_fill_size <= size);
  // Pad remaining bytes
  if (zero_fill_size)
    memset(((char *)buf) + bytes_copied, 0, zero_fill_size);

  return bytes_copied + zero_fill_size;
}

void ProcessElfCore::Clear() {
  m_thread_list.Clear();
  m_os = llvm::Triple::UnknownOS;

  SetUnixSignals(std::make_shared<UnixSignals>());
}

void ProcessElfCore::Initialize() {
  static llvm::once_flag g_once_flag;

  llvm::call_once(g_once_flag, []() {
    PluginManager::RegisterPlugin(GetPluginNameStatic(),
                                  GetPluginDescriptionStatic(), CreateInstance);
  });
}

lldb::addr_t ProcessElfCore::GetImageInfoAddress() {
  ObjectFile *obj_file = GetTarget().GetExecutableModule()->GetObjectFile();
  Address addr = obj_file->GetImageInfoAddress(&GetTarget());

  if (addr.IsValid())
    return addr.GetLoadAddress(&GetTarget());
  return LLDB_INVALID_ADDRESS;
}

/// Core files PT_NOTE segment descriptor types
enum {
  NT_PRSTATUS = 1,
  NT_FPREGSET,
  NT_PRPSINFO,
  NT_TASKSTRUCT,
  NT_PLATFORM,
  NT_AUXV,
  NT_FILE = 0x46494c45,
  NT_PRXFPREG = 0x46e62b7f,
  NT_SIGINFO = 0x53494749,
  NT_OPENBSD_PROCINFO = 10,
  NT_OPENBSD_AUXV = 11,
  NT_OPENBSD_REGS = 20,
  NT_OPENBSD_FPREGS = 21,
};

namespace FREEBSD {

enum {
  NT_PRSTATUS = 1,
  NT_FPREGSET,
  NT_PRPSINFO,
  NT_THRMISC = 7,
  NT_PROCSTAT_AUXV = 16,
  NT_PPC_VMX = 0x100
};
}

namespace NETBSD {

enum { NT_PROCINFO = 1, NT_AUXV, NT_AMD64_REGS = 33, NT_AMD64_FPREGS = 35 };
}

// Parse a FreeBSD NT_PRSTATUS note - see FreeBSD sys/procfs.h for details.
static void ParseFreeBSDPrStatus(ThreadData &thread_data, DataExtractor &data,
                                 ArchSpec &arch) {
  lldb::offset_t offset = 0;
  bool lp64 = (arch.GetMachine() == llvm::Triple::aarch64 ||
               arch.GetMachine() == llvm::Triple::mips64 ||
               arch.GetMachine() == llvm::Triple::ppc64 ||
               arch.GetMachine() == llvm::Triple::x86_64);
  int pr_version = data.GetU32(&offset);

  Log *log(GetLogIfAllCategoriesSet(LIBLLDB_LOG_PROCESS));
  if (log) {
    if (pr_version > 1)
      log->Printf("FreeBSD PRSTATUS unexpected version %d", pr_version);
  }

  // Skip padding, pr_statussz, pr_gregsetsz, pr_fpregsetsz, pr_osreldate
  if (lp64)
    offset += 32;
  else
    offset += 16;

  thread_data.signo = data.GetU32(&offset); // pr_cursig
  thread_data.tid = data.GetU32(&offset);   // pr_pid
  if (lp64)
    offset += 4;

  size_t len = data.GetByteSize() - offset;
  thread_data.gpregset = DataExtractor(data, offset, len);
}

static void ParseFreeBSDThrMisc(ThreadData &thread_data, DataExtractor &data) {
  lldb::offset_t offset = 0;
  thread_data.name = data.GetCStr(&offset, 20);
}

static void ParseNetBSDProcInfo(ThreadData &thread_data, DataExtractor &data) {
  lldb::offset_t offset = 0;

  int version = data.GetU32(&offset);
  if (version != 1)
    return;

  offset += 4;
  thread_data.signo = data.GetU32(&offset);
}

static void ParseOpenBSDProcInfo(ThreadData &thread_data, DataExtractor &data) {
  lldb::offset_t offset = 0;

  int version = data.GetU32(&offset);
  if (version != 1)
    return;

  offset += 4;
  thread_data.signo = data.GetU32(&offset);
}

/// Parse Thread context from PT_NOTE segment and store it in the thread list
/// Notes:
/// 1) A PT_NOTE segment is composed of one or more NOTE entries.
/// 2) NOTE Entry contains a standard header followed by variable size data.
///   (see ELFNote structure)
/// 3) A Thread Context in a core file usually described by 3 NOTE entries.
///    a) NT_PRSTATUS - Register context
///    b) NT_PRPSINFO - Process info(pid..)
///    c) NT_FPREGSET - Floating point registers
/// 4) The NOTE entries can be in any order
/// 5) If a core file contains multiple thread contexts then there is two data
/// forms
///    a) Each thread context(2 or more NOTE entries) contained in its own
///    segment (PT_NOTE)
///    b) All thread context is stored in a single segment(PT_NOTE).
///        This case is little tricker since while parsing we have to find where
///        the
///        new thread starts. The current implementation marks beginning of
///        new thread when it finds NT_PRSTATUS or NT_PRPSINFO NOTE entry.
///    For case (b) there may be either one NT_PRPSINFO per thread, or a single
///    one that applies to all threads (depending on the platform type).
Status ProcessElfCore::ParseThreadContextsFromNoteSegment(
    const elf::ELFProgramHeader *segment_header, DataExtractor segment_data) {
  assert(segment_header && segment_header->p_type == llvm::ELF::PT_NOTE);

  lldb::offset_t offset = 0;
  std::unique_ptr<ThreadData> thread_data(new ThreadData);
  bool have_prstatus = false;
  bool have_prpsinfo = false;

  ArchSpec arch = GetArchitecture();
  ELFLinuxPrPsInfo prpsinfo;
  ELFLinuxPrStatus prstatus;
  ELFLinuxSigInfo siginfo;
  size_t header_size;
  size_t len;
  Status error;

  // Loop through the NOTE entires in the segment
  while (offset < segment_header->p_filesz) {
    ELFNote note = ELFNote();
    note.Parse(segment_data, &offset);

    // Beginning of new thread
    if ((note.n_type == NT_PRSTATUS && have_prstatus) ||
        (note.n_type == NT_PRPSINFO && have_prpsinfo)) {
      assert(thread_data->gpregset.GetByteSize() > 0);
      // Add the new thread to thread list
      m_thread_data.push_back(*thread_data);
      *thread_data = ThreadData();
      have_prstatus = false;
      have_prpsinfo = false;
    }

    size_t note_start, note_size;
    note_start = offset;
    note_size = llvm::alignTo(note.n_descsz, 4);

    // Store the NOTE information in the current thread
    DataExtractor note_data(segment_data, note_start, note_size);
    note_data.SetAddressByteSize(
        m_core_module_sp->GetArchitecture().GetAddressByteSize());
    if (note.n_name == "FreeBSD") {
      m_os = llvm::Triple::FreeBSD;
      switch (note.n_type) {
      case FREEBSD::NT_PRSTATUS:
        have_prstatus = true;
        ParseFreeBSDPrStatus(*thread_data, note_data, arch);
        break;
      case FREEBSD::NT_FPREGSET:
        thread_data->fpregset = note_data;
        break;
      case FREEBSD::NT_PRPSINFO:
        have_prpsinfo = true;
        break;
      case FREEBSD::NT_THRMISC:
        ParseFreeBSDThrMisc(*thread_data, note_data);
        break;
      case FREEBSD::NT_PROCSTAT_AUXV:
        // FIXME: FreeBSD sticks an int at the beginning of the note
        m_auxv = DataExtractor(segment_data, note_start + 4, note_size - 4);
        break;
      case FREEBSD::NT_PPC_VMX:
        thread_data->vregset = note_data;
        break;
      default:
        break;
      }
    } else if (note.n_name.substr(0, 11) == "NetBSD-CORE") {
      // NetBSD per-thread information is stored in notes named
      // "NetBSD-CORE@nnn" so match on the initial part of the string.
      m_os = llvm::Triple::NetBSD;
      if (note.n_type == NETBSD::NT_PROCINFO) {
        ParseNetBSDProcInfo(*thread_data, note_data);
      } else if (note.n_type == NETBSD::NT_AUXV) {
        m_auxv = DataExtractor(note_data);
      } else if (arch.GetMachine() == llvm::Triple::x86_64 &&
                 note.n_type == NETBSD::NT_AMD64_REGS) {
        thread_data->gpregset = note_data;
      } else if (arch.GetMachine() == llvm::Triple::x86_64 &&
                 note.n_type == NETBSD::NT_AMD64_FPREGS) {
        thread_data->fpregset = note_data;
      }
    } else if (note.n_name.substr(0, 7) == "OpenBSD") {
      // OpenBSD per-thread information is stored in notes named
      // "OpenBSD@nnn" so match on the initial part of the string.
      m_os = llvm::Triple::OpenBSD;
      switch (note.n_type) {
      case NT_OPENBSD_PROCINFO:
        ParseOpenBSDProcInfo(*thread_data, note_data);
        break;
      case NT_OPENBSD_AUXV:
        m_auxv = DataExtractor(note_data);
        break;
      case NT_OPENBSD_REGS:
        thread_data->gpregset = note_data;
        break;
      case NT_OPENBSD_FPREGS:
        thread_data->fpregset = note_data;
        break;
      }
    } else if (note.n_name == "CORE") {
      switch (note.n_type) {
      case NT_PRSTATUS:
        have_prstatus = true;
        error = prstatus.Parse(note_data, arch);
        if (error.Fail())
          return error;
        thread_data->prstatus_sig = prstatus.pr_cursig;
        thread_data->tid = prstatus.pr_pid;
        header_size = ELFLinuxPrStatus::GetSize(arch);
        len = note_data.GetByteSize() - header_size;
        thread_data->gpregset = DataExtractor(note_data, header_size, len);
        break;
      case NT_FPREGSET:
        // In a i386 core file NT_FPREGSET is present, but it's not the result
        // of the FXSAVE instruction like in 64 bit files.
        // The result from FXSAVE is in NT_PRXFPREG for i386 core files
        if (arch.GetCore() == ArchSpec::eCore_x86_64_x86_64)
          thread_data->fpregset = note_data;
        else if(arch.IsMIPS())
          thread_data->fpregset = note_data;
        break;
      case NT_PRPSINFO:
        have_prpsinfo = true;
        error = prpsinfo.Parse(note_data, arch);
        if (error.Fail())
          return error;
        thread_data->name = prpsinfo.pr_fname;
        SetID(prpsinfo.pr_pid);
        break;
      case NT_AUXV:
        m_auxv = DataExtractor(note_data);
        break;
      case NT_FILE: {
        m_nt_file_entries.clear();
        lldb::offset_t offset = 0;
        const uint64_t count = note_data.GetAddress(&offset);
        note_data.GetAddress(&offset); // Skip page size
        for (uint64_t i = 0; i < count; ++i) {
          NT_FILE_Entry entry;
          entry.start = note_data.GetAddress(&offset);
          entry.end = note_data.GetAddress(&offset);
          entry.file_ofs = note_data.GetAddress(&offset);
          m_nt_file_entries.push_back(entry);
        }
        for (uint64_t i = 0; i < count; ++i) {
          const char *path = note_data.GetCStr(&offset);
          if (path && path[0])
            m_nt_file_entries[i].path.SetCString(path);
        }
      } break;
      case NT_SIGINFO: {
        error = siginfo.Parse(note_data, arch);
        if (error.Fail())
          return error;
        thread_data->signo = siginfo.si_signo;
      } break;
      default:
        break;
      }
    } else if (note.n_name == "LINUX") {
      switch (note.n_type) {
      case NT_PRXFPREG:
        thread_data->fpregset = note_data;
      }
    }

    offset += note_size;
  }
  // Add last entry in the note section
  if (thread_data && thread_data->gpregset.GetByteSize() > 0) {
    m_thread_data.push_back(*thread_data);
  }

  return error;
}

uint32_t ProcessElfCore::GetNumThreadContexts() {
  if (!m_thread_data_valid)
    DoLoadCore();
  return m_thread_data.size();
}

ArchSpec ProcessElfCore::GetArchitecture() {
  ObjectFileELF *core_file =
      (ObjectFileELF *)(m_core_module_sp->GetObjectFile());
  ArchSpec arch;
  core_file->GetArchitecture(arch);

  ArchSpec target_arch = GetTarget().GetArchitecture();
  
  if (target_arch.IsMIPS())
    return target_arch;

  return arch;
}

const lldb::DataBufferSP ProcessElfCore::GetAuxvData() {
  const uint8_t *start = m_auxv.GetDataStart();
  size_t len = m_auxv.GetByteSize();
  lldb::DataBufferSP buffer(new lldb_private::DataBufferHeap(start, len));
  return buffer;
}

bool ProcessElfCore::GetProcessInfo(ProcessInstanceInfo &info) {
  info.Clear();
  info.SetProcessID(GetID());
  info.SetArchitecture(GetArchitecture());
  lldb::ModuleSP module_sp = GetTarget().GetExecutableModule();
  if (module_sp) {
    const bool add_exe_file_as_first_arg = false;
    info.SetExecutableFile(GetTarget().GetExecutableModule()->GetFileSpec(),
                           add_exe_file_as_first_arg);
  }
  return true;
}
