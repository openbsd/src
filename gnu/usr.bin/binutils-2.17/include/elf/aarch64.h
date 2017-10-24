/* aarch64 ELF support for BFD.
 * Copyright (c) 2017 Philip Guenther <guenther@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _ELF_AARCH64_H
#define _ELF_AARCH64_H

#include "elf/reloc-macros.h"

START_RELOC_NUMBERS (elf_aarch64_reloc_type )
     RELOC_NUMBER (R_AARCH64_NONE,            0)   /* No reloc */
     RELOC_NUMBER (R_AARCH64_ABS64,         257)   /* Direct 64 bit */
     RELOC_NUMBER (R_AARCH64_COPY,         1024)   /* Copy symbol at runtime */
     RELOC_NUMBER (R_AARCH64_GLOB_DAT,     1025)   /* Create GOT entry */
     RELOC_NUMBER (R_AARCH64_JUMP_SLOT,    1026)   /* Create PLT entry */
     RELOC_NUMBER (R_AARCH64_RELATIVE,     1027)   /* Adjust by object base */
     RELOC_NUMBER (R_AARCH64_TLS_DTPREL64, 1028)   /* Dynamic TLS offset */
     RELOC_NUMBER (R_AARCH64_TLS_DTPMOD64, 1029)   /* Dynamic TLS module */
     RELOC_NUMBER (R_AARCH64_TLS_TPREL64,  1030)   /* Thread pointer relative */
     RELOC_NUMBER (R_AARCH64_TLSDESC,      1031)   /* TLS descriptor */
     RELOC_NUMBER (R_AARCH64_IRELATIVE,    1032)   /* Indirect relative */
END_RELOC_NUMBERS (R_AARCH64_max)

#endif
