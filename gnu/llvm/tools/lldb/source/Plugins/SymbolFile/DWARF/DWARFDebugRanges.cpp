//===-- DWARFDebugRanges.cpp ------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "DWARFDebugRanges.h"
#include "SymbolFileDWARF.h"
#include "lldb/Utility/Stream.h"
#include <assert.h>

using namespace lldb_private;
using namespace std;

static dw_addr_t GetBaseAddressMarker(uint32_t addr_size) {
  switch(addr_size) {
    case 2:
      return 0xffff;
    case 4:
      return 0xffffffff;
    case 8:
      return 0xffffffffffffffff;
  }
  llvm_unreachable("GetBaseAddressMarker unsupported address size.");
}

DWARFDebugRanges::DWARFDebugRanges() : m_range_map() {}

DWARFDebugRanges::~DWARFDebugRanges() {}

void DWARFDebugRanges::Extract(SymbolFileDWARF *dwarf2Data) {
  DWARFRangeList range_list;
  lldb::offset_t offset = 0;
  dw_offset_t debug_ranges_offset = offset;
  while (Extract(dwarf2Data, &offset, range_list)) {
    range_list.Sort();
    m_range_map[debug_ranges_offset] = range_list;
    debug_ranges_offset = offset;
  }
}

bool DWARFDebugRanges::Extract(SymbolFileDWARF *dwarf2Data,
                               lldb::offset_t *offset_ptr,
                               DWARFRangeList &range_list) {
  range_list.Clear();

  lldb::offset_t range_offset = *offset_ptr;
  const DWARFDataExtractor &debug_ranges_data =
      dwarf2Data->get_debug_ranges_data();
  uint32_t addr_size = debug_ranges_data.GetAddressByteSize();
  dw_addr_t base_addr = 0;
  dw_addr_t base_addr_marker = GetBaseAddressMarker(addr_size);

  while (
      debug_ranges_data.ValidOffsetForDataOfSize(*offset_ptr, 2 * addr_size)) {
    dw_addr_t begin = debug_ranges_data.GetMaxU64(offset_ptr, addr_size);
    dw_addr_t end = debug_ranges_data.GetMaxU64(offset_ptr, addr_size);

    if (!begin && !end) {
      // End of range list
      break;
    }

    if (begin == base_addr_marker) {
      base_addr = end;
      continue;
    }

    // Filter out empty ranges
    if (begin < end)
      range_list.Append(DWARFRangeList::Entry(begin + base_addr, end - begin));
  }

  // Make sure we consumed at least something
  return range_offset != *offset_ptr;
}

void DWARFDebugRanges::Dump(Stream &s,
                            const DWARFDataExtractor &debug_ranges_data,
                            lldb::offset_t *offset_ptr,
                            dw_addr_t cu_base_addr) {
  uint32_t addr_size = s.GetAddressByteSize();

  dw_addr_t base_addr = cu_base_addr;
  while (
      debug_ranges_data.ValidOffsetForDataOfSize(*offset_ptr, 2 * addr_size)) {
    dw_addr_t begin = debug_ranges_data.GetMaxU64(offset_ptr, addr_size);
    dw_addr_t end = debug_ranges_data.GetMaxU64(offset_ptr, addr_size);
    // Extend 4 byte addresses that consists of 32 bits of 1's to be 64 bits of
    // ones
    if (begin == 0xFFFFFFFFull && addr_size == 4)
      begin = LLDB_INVALID_ADDRESS;

    s.Indent();
    if (begin == 0 && end == 0) {
      s.PutCString(" End");
      break;
    } else if (begin == LLDB_INVALID_ADDRESS) {
      // A base address selection entry
      base_addr = end;
      s.Address(base_addr, sizeof(dw_addr_t), " Base address = ");
    } else {
      // Convert from offset to an address
      dw_addr_t begin_addr = begin + base_addr;
      dw_addr_t end_addr = end + base_addr;

      s.AddressRange(begin_addr, end_addr, sizeof(dw_addr_t), NULL);
    }
  }
}

bool DWARFDebugRanges::FindRanges(dw_addr_t debug_ranges_base,
                                  dw_offset_t debug_ranges_offset,
                                  DWARFRangeList &range_list) const {
  dw_addr_t debug_ranges_address = debug_ranges_base + debug_ranges_offset;
  range_map_const_iterator pos = m_range_map.find(debug_ranges_address);
  if (pos != m_range_map.end()) {
    range_list = pos->second;
    return true;
  }
  return false;
}
