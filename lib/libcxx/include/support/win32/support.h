// -*- C++ -*-
//===----------------------- support/win32/support.h ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP_SUPPORT_WIN32_SUPPORT_H
#define _LIBCPP_SUPPORT_WIN32_SUPPORT_H

// Functions and constants used in libc++ that
// are missing from the Windows C library.

#include <wchar.h> // mbstate_t
#include <cstdarg> // va_ macros
// "builtins" not implemented here for Clang or GCC as they provide
// implementations. Assuming required for elsewhere else, certainly MSVC.
#if defined(_LIBCPP_MSVC)
#include <intrin.h>
#endif
#if defined(_LIBCPP_MSVCRT)
#include <xlocinfo.h>
#endif
#define swprintf _snwprintf
#define vswprintf _vsnwprintf

#ifndef NOMINMAX
#define NOMINMAX
#endif

// The mingw headers already define these as static.
#ifndef __MINGW32__
extern "C" {

int vasprintf(char **sptr, const char *__restrict fmt, va_list ap);
int asprintf(char **sptr, const char *__restrict fmt, ...);
size_t mbsnrtowcs(wchar_t *__restrict dst, const char **__restrict src,
                  size_t nmc, size_t len, mbstate_t *__restrict ps);
size_t wcsnrtombs(char *__restrict dst, const wchar_t **__restrict src,
                  size_t nwc, size_t len, mbstate_t *__restrict ps);
}
#endif // __MINGW32__

#if defined(_LIBCPP_MSVCRT)
#define snprintf _snprintf
#define atoll _atoi64
#define strtoll _strtoi64
#define strtoull _strtoui64
#define wcstoll _wcstoi64
#define wcstoull _wcstoui64
_LIBCPP_ALWAYS_INLINE float strtof(const char *nptr, char **endptr)
{
  return _Stof(nptr, endptr, 0);
}
_LIBCPP_ALWAYS_INLINE double strtod(const char *nptr, char **endptr)
{
  return _Stod(nptr, endptr, 0);
}
_LIBCPP_ALWAYS_INLINE long double strtold(const char *nptr, char **endptr)
{
  return _Stold(nptr, endptr, 0);
}

#define _Exit _exit
#endif

#if defined(_LIBCPP_MSVC)

// Bit builtin's make these assumptions when calling _BitScanForward/Reverse
// etc. These assumptions are expected to be true for Win32/Win64 which this
// file supports.
static_assert(sizeof(unsigned long long) == 8, "");
static_assert(sizeof(unsigned long) == 4, "");
static_assert(sizeof(unsigned int) == 4, "");

_LIBCPP_ALWAYS_INLINE int __builtin_popcount(unsigned int x)
{
  // Binary: 0101...
  static const unsigned int m1 = 0x55555555;
  // Binary: 00110011..
  static const unsigned int m2 = 0x33333333;
  // Binary:  4 zeros,  4 ones ...
  static const unsigned int m4 = 0x0f0f0f0f;
  // The sum of 256 to the power of 0,1,2,3...
  static const unsigned int h01 = 0x01010101;
  // Put count of each 2 bits into those 2 bits.
  x -= (x >> 1) & m1;
  // Put count of each 4 bits into those 4 bits.
  x = (x & m2) + ((x >> 2) & m2);
  // Put count of each 8 bits into those 8 bits.
  x = (x + (x >> 4)) & m4;
  // Returns left 8 bits of x + (x<<8) + (x<<16) + (x<<24).
  return (x * h01) >> 24;
}

_LIBCPP_ALWAYS_INLINE int __builtin_popcountl(unsigned long x)
{
  return __builtin_popcount(static_cast<int>(x));
}

_LIBCPP_ALWAYS_INLINE int __builtin_popcountll(unsigned long long x)
{
  // Binary: 0101...
  static const unsigned long long m1 = 0x5555555555555555;
  // Binary: 00110011..
  static const unsigned long long m2 = 0x3333333333333333;
  // Binary:  4 zeros,  4 ones ...
  static const unsigned long long m4 = 0x0f0f0f0f0f0f0f0f;
  // The sum of 256 to the power of 0,1,2,3...
  static const unsigned long long h01 = 0x0101010101010101;
  // Put count of each 2 bits into those 2 bits.
  x -= (x >> 1) & m1;
  // Put count of each 4 bits into those 4 bits.
  x = (x & m2) + ((x >> 2) & m2);
  // Put count of each 8 bits into those 8 bits.
  x = (x + (x >> 4)) & m4;
  // Returns left 8 bits of x + (x<<8) + (x<<16) + (x<<24) + ...
  return static_cast<int>((x * h01) >> 56);
}

// Returns the number of trailing 0-bits in x, starting at the least significant
// bit position. If x is 0, the result is undefined.
_LIBCPP_ALWAYS_INLINE int __builtin_ctzll(unsigned long long mask)
{
  unsigned long where;
// Search from LSB to MSB for first set bit.
// Returns zero if no set bit is found.
#if defined(_WIN64)
  if (_BitScanForward64(&where, mask))
    return static_cast<int>(where);
#elif defined(_WIN32)
  // Win32 doesn't have _BitScanForward64 so emulate it with two 32 bit calls.
  // Scan the Low Word.
  if (_BitScanForward(&where, static_cast<unsigned long>(mask)))
    return static_cast<int>(where);
  // Scan the High Word.
  if (_BitScanForward(&where, static_cast<unsigned long>(mask >> 32)))
    return static_cast<int>(where + 32); // Create a bit offset from the LSB.
#else
#error "Implementation of __builtin_ctzll required"
#endif
  return 64;
}

_LIBCPP_ALWAYS_INLINE int __builtin_ctzl(unsigned long mask)
{
  unsigned long where;
  // Search from LSB to MSB for first set bit.
  // Returns zero if no set bit is found.
  if (_BitScanForward(&where, mask))
    return static_cast<int>(where);
  return 32;
}

_LIBCPP_ALWAYS_INLINE int __builtin_ctz(unsigned int mask)
{
  // Win32 and Win64 expectations.
  static_assert(sizeof(mask) == 4, "");
  static_assert(sizeof(unsigned long) == 4, "");
  return __builtin_ctzl(static_cast<unsigned long>(mask));
}

// Returns the number of leading 0-bits in x, starting at the most significant
// bit position. If x is 0, the result is undefined.
_LIBCPP_ALWAYS_INLINE int __builtin_clzll(unsigned long long mask)
{
  unsigned long where;
// BitScanReverse scans from MSB to LSB for first set bit.
// Returns 0 if no set bit is found.
#if defined(_WIN64)
  if (_BitScanReverse64(&where, mask))
    return static_cast<int>(63 - where);
#elif defined(_WIN32)
  // Scan the high 32 bits.
  if (_BitScanReverse(&where, static_cast<unsigned long>(mask >> 32)))
    return static_cast<int>(63 -
                            (where + 32)); // Create a bit offset from the MSB.
  // Scan the low 32 bits.
  if (_BitScanReverse(&where, static_cast<unsigned long>(mask)))
    return static_cast<int>(63 - where);
#else
#error "Implementation of __builtin_clzll required"
#endif
  return 64; // Undefined Behavior.
}

_LIBCPP_ALWAYS_INLINE int __builtin_clzl(unsigned long mask)
{
  unsigned long where;
  // Search from LSB to MSB for first set bit.
  // Returns zero if no set bit is found.
  if (_BitScanReverse(&where, mask))
    return static_cast<int>(31 - where);
  return 32; // Undefined Behavior.
}

_LIBCPP_ALWAYS_INLINE int __builtin_clz(unsigned int x)
{
  return __builtin_clzl(x);
}
#endif // _LIBCPP_MSVC

#endif // _LIBCPP_SUPPORT_WIN32_SUPPORT_H
