//===-- UUID.h --------------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_UTILITY_UUID_H
#define LLDB_UTILITY_UUID_H

// C Includes
// C++ Includes
#include <stddef.h>
#include <stdint.h>
#include <string>

namespace llvm {
  class StringRef;
}

namespace lldb_private {

  class Stream;

class UUID {
public:
  // Most UUIDs are 16 bytes, but some Linux build-ids (SHA1) are 20.
  typedef uint8_t ValueType[20];

  //------------------------------------------------------------------
  // Constructors and Destructors
  //------------------------------------------------------------------
  UUID();
  UUID(const UUID &rhs);
  UUID(const void *uuid_bytes, uint32_t num_uuid_bytes);

  ~UUID();

  const UUID &operator=(const UUID &rhs);

  void Clear();

  void Dump(Stream *s) const;

  const void *GetBytes() const;

  size_t GetByteSize();

  bool IsValid() const;

  bool SetBytes(const void *uuid_bytes, uint32_t num_uuid_bytes = 16);

  std::string GetAsString(const char *separator = nullptr) const;

  size_t SetFromStringRef(llvm::StringRef str, uint32_t num_uuid_bytes = 16);
  size_t SetFromCString(const char *c_str, uint32_t num_uuid_bytes = 16);

  // Decode as many UUID bytes (up to 16) as possible from the C string "cstr"
  // This is used for auto completion where a partial UUID might have been
  // typed in. It
  //------------------------------------------------------------------
  /// Decode as many UUID bytes (up to 16) as possible from the C
  /// string \a cstr.
  ///
  /// @param[in] cstr
  ///     A NULL terminate C string that points at a UUID string value
  ///     (no leading spaces). The string must contain only hex
  ///     characters and optionally can contain the '-' sepearators.
  ///
  /// @param[in] uuid_bytes
  ///     A buffer of bytes that will contain a full or patially
  ///     decoded UUID.
  ///
  /// @return
  ///     The original string, with all decoded bytes removed.
  //------------------------------------------------------------------
  static llvm::StringRef
  DecodeUUIDBytesFromString(llvm::StringRef str, ValueType &uuid_bytes,
                            uint32_t &bytes_decoded,
                            uint32_t num_uuid_bytes = 16);

protected:
  //------------------------------------------------------------------
  // Classes that inherit from UUID can see and modify these
  //------------------------------------------------------------------
  uint32_t m_num_uuid_bytes; // Should be 16 or 20
  ValueType m_uuid;
};

bool operator==(const UUID &lhs, const UUID &rhs);
bool operator!=(const UUID &lhs, const UUID &rhs);
bool operator<(const UUID &lhs, const UUID &rhs);
bool operator<=(const UUID &lhs, const UUID &rhs);
bool operator>(const UUID &lhs, const UUID &rhs);
bool operator>=(const UUID &lhs, const UUID &rhs);

} // namespace lldb_private

#endif // LLDB_UTILITY_UUID_H
