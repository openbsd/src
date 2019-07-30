//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP_SUPPORT_NEWLIB_XLOCALE_H
#define _LIBCPP_SUPPORT_NEWLIB_XLOCALE_H

#if defined(_NEWLIB_VERSION) || defined(__OpenBSD__)

#include <cstdlib>
#include <clocale>
#include <cwctype>
#include <ctype.h>
#ifndef __OpenBSD__
#include <support/xlocale/__nop_locale_mgmt.h>
#include <support/xlocale/__posix_l_fallback.h>
#endif
#include <support/xlocale/__strtonum_fallback.h>

#endif // _NEWLIB_VERSION

#endif
