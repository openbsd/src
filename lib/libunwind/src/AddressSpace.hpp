//===------------------------- AddressSpace.hpp ---------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//
// Abstracts accessing local vs remote address spaces.
//
//===----------------------------------------------------------------------===//

#ifndef __ADDRESSSPACE_HPP__
#define __ADDRESSSPACE_HPP__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _LIBUNWIND_IS_BAREMETAL
#include <dlfcn.h>
#endif

#ifdef __APPLE__
#include <mach-o/getsect.h>
namespace libunwind {
   bool checkKeyMgrRegisteredFDEs(uintptr_t targetAddr, void *&fde);
}
#endif

#include "libunwind.h"
#include "config.h"
#include "dwarf2.h"
#include "Registers.hpp"

#if _LIBUNWIND_ARM_EHABI
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)

#include <link.h>
typedef void *_Unwind_Ptr;

#elif defined(__linux__)

typedef long unsigned int *_Unwind_Ptr;
extern "C" _Unwind_Ptr __gnu_Unwind_Find_exidx(_Unwind_Ptr addr, int *len);

// Emulate the BSD dl_unwind_find_exidx API when on a GNU libdl system.
#define dl_unwind_find_exidx __gnu_Unwind_Find_exidx

#elif !defined(_LIBUNWIND_IS_BAREMETAL)
#include <link.h>
#else // !defined(_LIBUNWIND_IS_BAREMETAL)
// When statically linked on bare-metal, the symbols for the EH table are looked
// up without going through the dynamic loader.
struct EHTEntry {
  uint32_t functionOffset;
  uint32_t unwindOpcodes;
};
extern EHTEntry __exidx_start;
extern EHTEntry __exidx_end;
#endif // !defined(_LIBUNWIND_IS_BAREMETAL)
#endif // _LIBUNWIND_ARM_EHABI

#if defined(__CloudABI__) || defined(__FreeBSD__) || defined(__linux__) ||	\
    defined(__NetBSD__) || defined(__OpenBSD__)
#if _LIBUNWIND_SUPPORT_DWARF_UNWIND && _LIBUNWIND_SUPPORT_DWARF_INDEX
#include <link.h>
// Macro for machine-independent access to the ELF program headers. This
// macro is not available on some systems (e.g., FreeBSD). On these
// systems the data structures are just called Elf_XXX. Define ElfW()
// locally.
#if !defined(ElfW)
#define ElfW(type) Elf_##type
#endif
#include "EHHeaderParser.hpp"
#endif
#endif

namespace libunwind {

/// Used by findUnwindSections() to return info about needed sections.
struct UnwindInfoSections {
#if _LIBUNWIND_SUPPORT_DWARF_UNWIND || _LIBUNWIND_SUPPORT_DWARF_INDEX ||       \
    _LIBUNWIND_SUPPORT_COMPACT_UNWIND
  // No dso_base for ARM EHABI.
  uintptr_t       dso_base;
#endif
#if _LIBUNWIND_SUPPORT_DWARF_UNWIND
  uintptr_t       dwarf_section;
  uintptr_t       dwarf_section_length;
#endif
#if _LIBUNWIND_SUPPORT_DWARF_INDEX
  uintptr_t       dwarf_index_section;
  uintptr_t       dwarf_index_section_length;
#endif
#if _LIBUNWIND_SUPPORT_COMPACT_UNWIND
  uintptr_t       compact_unwind_section;
  uintptr_t       compact_unwind_section_length;
#endif
#if _LIBUNWIND_ARM_EHABI
  uintptr_t       arm_section;
  uintptr_t       arm_section_length;
#endif
};


/// LocalAddressSpace is used as a template parameter to UnwindCursor when
/// unwinding a thread in the same process.  The wrappers compile away,
/// making local unwinds fast.
class __attribute__((visibility("hidden"))) LocalAddressSpace {
public:
#ifdef __LP64__
  typedef uint64_t pint_t;
  typedef int64_t  sint_t;
#else
  typedef uint32_t pint_t;
  typedef int32_t  sint_t;
#endif
  uint8_t         get8(pint_t addr) {
    uint8_t val;
    memcpy(&val, (void *)addr, sizeof(val));
    return val;
  }
  uint16_t         get16(pint_t addr) {
    uint16_t val;
    memcpy(&val, (void *)addr, sizeof(val));
    return val;
  }
  uint32_t         get32(pint_t addr) {
    uint32_t val;
    memcpy(&val, (void *)addr, sizeof(val));
    return val;
  }
  uint64_t         get64(pint_t addr) {
    uint64_t val;
    memcpy(&val, (void *)addr, sizeof(val));
    return val;
  }
  double           getDouble(pint_t addr) {
    double val;
    memcpy(&val, (void *)addr, sizeof(val));
    return val;
  }
  v128             getVector(pint_t addr) {
    v128 val;
    memcpy(&val, (void *)addr, sizeof(val));
    return val;
  }
  uintptr_t       getP(pint_t addr);
  static uint64_t getULEB128(pint_t &addr, pint_t end);
  static int64_t  getSLEB128(pint_t &addr, pint_t end);

  pint_t getEncodedP(pint_t &addr, pint_t end, uint8_t encoding,
                     pint_t datarelBase = 0);
  bool findFunctionName(pint_t addr, char *buf, size_t bufLen,
                        unw_word_t *offset);
  bool findUnwindSections(pint_t targetAddr, UnwindInfoSections &info);
  bool findOtherFDE(pint_t targetAddr, pint_t &fde);

  static LocalAddressSpace sThisAddressSpace;
};

inline uintptr_t LocalAddressSpace::getP(pint_t addr) {
#ifdef __LP64__
  return get64(addr);
#else
  return get32(addr);
#endif
}

/// Read a ULEB128 into a 64-bit word.
inline uint64_t LocalAddressSpace::getULEB128(pint_t &addr, pint_t end) {
  const uint8_t *p = (uint8_t *)addr;
  const uint8_t *pend = (uint8_t *)end;
  uint64_t result = 0;
  int bit = 0;
  do {
    uint64_t b;

    if (p == pend)
      _LIBUNWIND_ABORT("truncated uleb128 expression");

    b = *p & 0x7f;

    if (bit >= 64 || b << bit >> bit != b) {
      _LIBUNWIND_ABORT("malformed uleb128 expression");
    } else {
      result |= b << bit;
      bit += 7;
    }
  } while (*p++ >= 0x80);
  addr = (pint_t) p;
  return result;
}

/// Read a SLEB128 into a 64-bit word.
inline int64_t LocalAddressSpace::getSLEB128(pint_t &addr, pint_t end) {
  const uint8_t *p = (uint8_t *)addr;
  const uint8_t *pend = (uint8_t *)end;
  int64_t result = 0;
  int bit = 0;
  uint8_t byte;
  do {
    if (p == pend)
      _LIBUNWIND_ABORT("truncated sleb128 expression");
    byte = *p++;
    result |= ((byte & 0x7f) << bit);
    bit += 7;
  } while (byte & 0x80);
  // sign extend negative numbers
  if ((byte & 0x40) != 0)
    result |= (-1LL) << bit;
  addr = (pint_t) p;
  return result;
}

inline LocalAddressSpace::pint_t
LocalAddressSpace::getEncodedP(pint_t &addr, pint_t end, uint8_t encoding,
                               pint_t datarelBase) {
  pint_t startAddr = addr;
  const uint8_t *p = (uint8_t *)addr;
  pint_t result;

  if (encoding == DW_EH_PE_omit) {
    return (pint_t)NULL;
  }

  // first get value
  switch (encoding & 0x0F) {
  case DW_EH_PE_ptr:
    result = getP(addr);
    p += sizeof(pint_t);
    addr = (pint_t) p;
    break;
  case DW_EH_PE_uleb128:
    result = (pint_t)getULEB128(addr, end);
    break;
  case DW_EH_PE_udata2:
    result = get16(addr);
    p += 2;
    addr = (pint_t) p;
    break;
  case DW_EH_PE_udata4:
    result = get32(addr);
    p += 4;
    addr = (pint_t) p;
    break;
  case DW_EH_PE_udata8:
    result = (pint_t)get64(addr);
    p += 8;
    addr = (pint_t) p;
    break;
  case DW_EH_PE_sleb128:
    result = (pint_t)getSLEB128(addr, end);
    break;
  case DW_EH_PE_sdata2:
    // Sign extend from signed 16-bit value.
    result = (pint_t)(int16_t)get16(addr);
    p += 2;
    addr = (pint_t) p;
    break;
  case DW_EH_PE_sdata4:
    // Sign extend from signed 32-bit value.
    result = (pint_t)(int32_t)get32(addr);
    p += 4;
    addr = (pint_t) p;
    break;
  case DW_EH_PE_sdata8:
    result = (pint_t)get64(addr);
    p += 8;
    addr = (pint_t) p;
    break;
  default:
    _LIBUNWIND_ABORT("unknown pointer encoding");
  }

  // then add relative offset
  switch (encoding & 0x70) {
  case DW_EH_PE_absptr:
    // do nothing
    break;
  case DW_EH_PE_pcrel:
    result += startAddr;
    break;
  case DW_EH_PE_textrel:
    _LIBUNWIND_ABORT("DW_EH_PE_textrel pointer encoding not supported");
    break;
  case DW_EH_PE_datarel:
    // DW_EH_PE_datarel is only valid in a few places, so the parameter has a
    // default value of 0, and we abort in the event that someone calls this
    // function with a datarelBase of 0 and DW_EH_PE_datarel encoding.
    if (datarelBase == 0)
      _LIBUNWIND_ABORT("DW_EH_PE_datarel is invalid with a datarelBase of 0");
    result += datarelBase;
    break;
  case DW_EH_PE_funcrel:
    _LIBUNWIND_ABORT("DW_EH_PE_funcrel pointer encoding not supported");
    break;
  case DW_EH_PE_aligned:
    _LIBUNWIND_ABORT("DW_EH_PE_aligned pointer encoding not supported");
    break;
  default:
    _LIBUNWIND_ABORT("unknown pointer encoding");
    break;
  }

  if (encoding & DW_EH_PE_indirect)
    result = getP(result);

  return result;
}

#ifdef __APPLE__ 
  struct dyld_unwind_sections
  {
    const struct mach_header*   mh;
    const void*                 dwarf_section;
    uintptr_t                   dwarf_section_length;
    const void*                 compact_unwind_section;
    uintptr_t                   compact_unwind_section_length;
  };
  #if (defined(__MAC_OS_X_VERSION_MIN_REQUIRED) \
                                 && (__MAC_OS_X_VERSION_MIN_REQUIRED >= 1070)) \
      || defined(__IPHONE_OS_VERSION_MIN_REQUIRED)
    // In 10.7.0 or later, libSystem.dylib implements this function.
    extern "C" bool _dyld_find_unwind_sections(void *, dyld_unwind_sections *);
  #else
    // In 10.6.x and earlier, we need to implement this functionality.
    static inline bool _dyld_find_unwind_sections(void* addr, 
                                                    dyld_unwind_sections* info) {
      // Find mach-o image containing address.
      Dl_info dlinfo;
      if (!dladdr(addr, &dlinfo))
        return false;
      const mach_header *mh = (const mach_header *)dlinfo.dli_saddr;
      
      // Find dwarf unwind section in that image.
      unsigned long size;
      const uint8_t *p = getsectiondata(mh, "__TEXT", "__eh_frame", &size);
      if (!p)
        return false;
      
      // Fill in return struct.
      info->mh = mh;
      info->dwarf_section = p;
      info->dwarf_section_length = size;
      info->compact_unwind_section = 0;
      info->compact_unwind_section_length = 0;
     
      return true;
    }
  #endif
#endif

inline bool LocalAddressSpace::findUnwindSections(pint_t targetAddr,
                                                  UnwindInfoSections &info) {
#ifdef __APPLE__
  dyld_unwind_sections dyldInfo;
  if (_dyld_find_unwind_sections((void *)targetAddr, &dyldInfo)) {
    info.dso_base                      = (uintptr_t)dyldInfo.mh;
 #if _LIBUNWIND_SUPPORT_DWARF_UNWIND
    info.dwarf_section                 = (uintptr_t)dyldInfo.dwarf_section;
    info.dwarf_section_length          = dyldInfo.dwarf_section_length;
 #endif
    info.compact_unwind_section        = (uintptr_t)dyldInfo.compact_unwind_section;
    info.compact_unwind_section_length = dyldInfo.compact_unwind_section_length;
    return true;
  }
#elif _LIBUNWIND_ARM_EHABI
 #ifdef _LIBUNWIND_IS_BAREMETAL
  // Bare metal is statically linked, so no need to ask the dynamic loader
  info.arm_section =        (uintptr_t)(&__exidx_start);
  info.arm_section_length = (uintptr_t)(&__exidx_end - &__exidx_start);
 #else
  int length = 0;
  info.arm_section = (uintptr_t) dl_unwind_find_exidx(
      (_Unwind_Ptr) targetAddr, &length);
  info.arm_section_length = (uintptr_t)length;
 #endif
  _LIBUNWIND_TRACE_UNWINDING("findUnwindSections: section %X length %x\n",
                             info.arm_section, info.arm_section_length);
  if (info.arm_section && info.arm_section_length)
    return true;
#elif _LIBUNWIND_SUPPORT_DWARF_UNWIND
#if _LIBUNWIND_SUPPORT_DWARF_INDEX
  struct dl_iterate_cb_data {
    LocalAddressSpace *addressSpace;
    UnwindInfoSections *sects;
    uintptr_t targetAddr;
  };

  dl_iterate_cb_data cb_data = {this, &info, targetAddr};
  int found = dl_iterate_phdr(
      [](struct dl_phdr_info *pinfo, size_t, void *data) -> int {
        auto cbdata = static_cast<dl_iterate_cb_data *>(data);
        size_t object_length;
        bool found_obj = false;
        bool found_hdr = false;

        assert(cbdata);
        assert(cbdata->sects);

        if (cbdata->targetAddr < pinfo->dlpi_addr) {
          return false;
        }

#if !defined(Elf_Half)
        typedef ElfW(Half) Elf_Half;
#endif
#if !defined(Elf_Phdr)
        typedef ElfW(Phdr) Elf_Phdr;
#endif

        for (Elf_Half i = 0; i < pinfo->dlpi_phnum; i++) {
          const Elf_Phdr *phdr = &pinfo->dlpi_phdr[i];
          if (phdr->p_type == PT_LOAD) {
            uintptr_t begin = pinfo->dlpi_addr + phdr->p_vaddr;
            uintptr_t end = begin + phdr->p_memsz;
            if (cbdata->targetAddr >= begin && cbdata->targetAddr < end) {
              cbdata->sects->dso_base = begin;
              object_length = phdr->p_memsz;
              found_obj = true;
            }
          } else if (phdr->p_type == PT_GNU_EH_FRAME) {
            EHHeaderParser<LocalAddressSpace>::EHHeaderInfo hdrInfo;
            uintptr_t eh_frame_hdr_start = pinfo->dlpi_addr + phdr->p_vaddr;
            cbdata->sects->dwarf_index_section = eh_frame_hdr_start;
            cbdata->sects->dwarf_index_section_length = phdr->p_memsz;
            EHHeaderParser<LocalAddressSpace>::decodeEHHdr(
                *cbdata->addressSpace, eh_frame_hdr_start, phdr->p_memsz,
                hdrInfo);
            cbdata->sects->dwarf_section = hdrInfo.eh_frame_ptr;
            found_hdr = true;
          }
        }

        if (found_obj && found_hdr) {
          cbdata->sects->dwarf_section_length = object_length;
          return true;
        } else {
          return false;
        }
      },
      &cb_data);
  return static_cast<bool>(found);
#else
#error "_LIBUNWIND_SUPPORT_DWARF_UNWIND requires _LIBUNWIND_SUPPORT_DWARF_INDEX on this platform."
#endif
#endif

  return false;
}


inline bool LocalAddressSpace::findOtherFDE(pint_t targetAddr, pint_t &fde) {
#ifdef __APPLE__
  return checkKeyMgrRegisteredFDEs(targetAddr, *((void**)&fde));
#else
  // TO DO: if OS has way to dynamically register FDEs, check that.
  (void)targetAddr;
  (void)fde;
  return false;
#endif
}

inline bool LocalAddressSpace::findFunctionName(pint_t addr, char *buf,
                                                size_t bufLen,
                                                unw_word_t *offset) {
#ifndef _LIBUNWIND_IS_BAREMETAL
  Dl_info dyldInfo;
  if (dladdr((void *)addr, &dyldInfo)) {
    if (dyldInfo.dli_sname != NULL) {
      snprintf(buf, bufLen, "%s", dyldInfo.dli_sname);
      *offset = (addr - (pint_t) dyldInfo.dli_saddr);
      return true;
    }
  }
#endif
  return false;
}



#ifdef UNW_REMOTE

/// OtherAddressSpace is used as a template parameter to UnwindCursor when
/// unwinding a thread in the another process.  The other process can be a
/// different endianness and a different pointer size which is handled by
/// the P template parameter.
template <typename P>
class OtherAddressSpace {
public:
  OtherAddressSpace(task_t task) : fTask(task) {}

  typedef typename P::uint_t pint_t;

  uint8_t   get8(pint_t addr);
  uint16_t  get16(pint_t addr);
  uint32_t  get32(pint_t addr);
  uint64_t  get64(pint_t addr);
  pint_t    getP(pint_t addr);
  uint64_t  getULEB128(pint_t &addr, pint_t end);
  int64_t   getSLEB128(pint_t &addr, pint_t end);
  pint_t    getEncodedP(pint_t &addr, pint_t end, uint8_t encoding,
                        pint_t datarelBase = 0);
  bool      findFunctionName(pint_t addr, char *buf, size_t bufLen,
                        unw_word_t *offset);
  bool      findUnwindSections(pint_t targetAddr, UnwindInfoSections &info);
  bool      findOtherFDE(pint_t targetAddr, pint_t &fde);
private:
  void *localCopy(pint_t addr);

  task_t fTask;
};

template <typename P> uint8_t OtherAddressSpace<P>::get8(pint_t addr) {
  return *((uint8_t *)localCopy(addr));
}

template <typename P> uint16_t OtherAddressSpace<P>::get16(pint_t addr) {
  return P::E::get16(*(uint16_t *)localCopy(addr));
}

template <typename P> uint32_t OtherAddressSpace<P>::get32(pint_t addr) {
  return P::E::get32(*(uint32_t *)localCopy(addr));
}

template <typename P> uint64_t OtherAddressSpace<P>::get64(pint_t addr) {
  return P::E::get64(*(uint64_t *)localCopy(addr));
}

template <typename P>
typename P::uint_t OtherAddressSpace<P>::getP(pint_t addr) {
  return P::getP(*(uint64_t *)localCopy(addr));
}

template <typename P>
uint64_t OtherAddressSpace<P>::getULEB128(pint_t &addr, pint_t end) {
  uintptr_t size = (end - addr);
  LocalAddressSpace::pint_t laddr = (LocalAddressSpace::pint_t) localCopy(addr);
  LocalAddressSpace::pint_t sladdr = laddr;
  uint64_t result = LocalAddressSpace::getULEB128(laddr, laddr + size);
  addr += (laddr - sladdr);
  return result;
}

template <typename P>
int64_t OtherAddressSpace<P>::getSLEB128(pint_t &addr, pint_t end) {
  uintptr_t size = (end - addr);
  LocalAddressSpace::pint_t laddr = (LocalAddressSpace::pint_t) localCopy(addr);
  LocalAddressSpace::pint_t sladdr = laddr;
  uint64_t result = LocalAddressSpace::getSLEB128(laddr, laddr + size);
  addr += (laddr - sladdr);
  return result;
}

template <typename P> void *OtherAddressSpace<P>::localCopy(pint_t addr) {
  // FIX ME
}

template <typename P>
bool OtherAddressSpace<P>::findFunctionName(pint_t addr, char *buf,
                                            size_t bufLen, unw_word_t *offset) {
  // FIX ME
}

/// unw_addr_space is the base class that abstract unw_addr_space_t type in
/// libunwind.h points to.
struct unw_addr_space {
  cpu_type_t cpuType;
  task_t taskPort;
};

/// unw_addr_space_i386 is the concrete instance that a unw_addr_space_t points
/// to when examining
/// a 32-bit intel process.
struct unw_addr_space_i386 : public unw_addr_space {
  unw_addr_space_i386(task_t task) : oas(task) {}
  OtherAddressSpace<Pointer32<LittleEndian> > oas;
};

/// unw_addr_space_x86_64 is the concrete instance that a unw_addr_space_t
/// points to when examining
/// a 64-bit intel process.
struct unw_addr_space_x86_64 : public unw_addr_space {
  unw_addr_space_x86_64(task_t task) : oas(task) {}
  OtherAddressSpace<Pointer64<LittleEndian> > oas;
};

/// unw_addr_space_ppc is the concrete instance that a unw_addr_space_t points
/// to when examining
/// a 32-bit PowerPC process.
struct unw_addr_space_ppc : public unw_addr_space {
  unw_addr_space_ppc(task_t task) : oas(task) {}
  OtherAddressSpace<Pointer32<BigEndian> > oas;
};

#endif // UNW_REMOTE

} // namespace libunwind

#endif // __ADDRESSSPACE_HPP__
