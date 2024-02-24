// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifdef __APPLE__
#   define S2N_BN_SYMBOL(NAME) _##NAME
#else
#   define S2N_BN_SYMBOL(name) name
#endif

#ifdef __CET__
#   include <cet.h>
#else
#   define _CET_ENDBR
#endif

#define S2N_BN_SYM_VISIBILITY_DIRECTIVE(name) .globl S2N_BN_SYMBOL(name)
#ifdef S2N_BN_HIDE_SYMBOLS
#   ifdef __APPLE__
#      define S2N_BN_SYM_PRIVACY_DIRECTIVE(name) .private_extern S2N_BN_SYMBOL(name)
#   else
#      define S2N_BN_SYM_PRIVACY_DIRECTIVE(name) .hidden S2N_BN_SYMBOL(name)
#   endif
#else
#   define S2N_BN_SYM_PRIVACY_DIRECTIVE(name)  /* NO-OP: S2N_BN_SYM_PRIVACY_DIRECTIVE */
#endif
