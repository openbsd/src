//===------------------------- __libunwind_config.h -----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef ____LIBUNWIND_CONFIG_H__
#define ____LIBUNWIND_CONFIG_H__

#if defined(__arm__) && !defined(__USING_SJLJ_EXCEPTIONS__) && \
    !defined(__ARM_DWARF_EH__)
#define _LIBUNWIND_ARM_EHABI 1
#else
#define _LIBUNWIND_ARM_EHABI 0
#endif

#if defined(_LIBUNWIND_IS_NATIVE_ONLY)
# if defined(__i386__)
#  define _LIBUNWIND_TARGET_I386 1
#  define _LIBUNWIND_CONTEXT_SIZE 8
#  define _LIBUNWIND_CURSOR_SIZE 19
#  define _LIBUNWIND_HIGHEST_DWARF_REGISTER 9
# elif defined(__x86_64__)
#  define _LIBUNWIND_TARGET_X86_64 1
#  define _LIBUNWIND_CONTEXT_SIZE 21
#  define _LIBUNWIND_CURSOR_SIZE 33
#  define _LIBUNWIND_HIGHEST_DWARF_REGISTER 17
# elif defined(__ppc__)
#  define _LIBUNWIND_TARGET_PPC 1
#  define _LIBUNWIND_CONTEXT_SIZE 117
#  define _LIBUNWIND_CURSOR_SIZE 128
#  define _LIBUNWIND_HIGHEST_DWARF_REGISTER 113
# elif defined(__aarch64__)
#  define _LIBUNWIND_TARGET_AARCH64 1
#  define _LIBUNWIND_CONTEXT_SIZE 66
#  define _LIBUNWIND_CURSOR_SIZE 78
#  define _LIBUNWIND_HIGHEST_DWARF_REGISTER 96
# elif defined(__arm__)
#  define _LIBUNWIND_TARGET_ARM 1
#  if defined(__ARM_WMMX)
#    define _LIBUNWIND_CONTEXT_SIZE 60
#    define _LIBUNWIND_CURSOR_SIZE 67
#  else
#    define _LIBUNWIND_CONTEXT_SIZE 42
#    define _LIBUNWIND_CURSOR_SIZE 49
#  endif
#  define _LIBUNWIND_HIGHEST_DWARF_REGISTER 96
# elif defined(__or1k__)
#  define _LIBUNWIND_TARGET_OR1K 1
#  define _LIBUNWIND_CONTEXT_SIZE 16
#  define _LIBUNWIND_CURSOR_SIZE 28
#  define _LIBUNWIND_HIGHEST_DWARF_REGISTER 32
# elif defined(__sparc__) && defined(__arch64__)
#  define _LIBUNWIND_TARGET_SPARC64 1
#  define _LIBUNWIND_CONTEXT_SIZE 33
#  define _LIBUNWIND_CURSOR_SIZE 45
#  define _LIBUNWIND_HIGHEST_DWARF_REGISTER 32
# else
#  error "Unsupported architecture."
# endif
#else // !_LIBUNWIND_IS_NATIVE_ONLY
# define _LIBUNWIND_TARGET_I386 1
# define _LIBUNWIND_TARGET_X86_64 1
# define _LIBUNWIND_TARGET_PPC 1
# define _LIBUNWIND_TARGET_AARCH64 1
# define _LIBUNWIND_TARGET_ARM 1
# define _LIBUNWIND_TARGET_OR1K 1
# define _LIBUNWIND_CONTEXT_SIZE 128
# define _LIBUNWIND_CURSOR_SIZE 140
# define _LIBUNWIND_HIGHEST_DWARF_REGISTER 120
#endif // _LIBUNWIND_IS_NATIVE_ONLY

#endif // ____LIBUNWIND_CONFIG_H__
