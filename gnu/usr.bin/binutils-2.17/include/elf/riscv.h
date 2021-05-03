/* RISC-V ELF support for BFD.
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

#ifndef _ELF_RISCV_H
#define _ELF_RISCV_H

#include "elf/reloc-macros.h"

START_RELOC_NUMBERS (elf_riscv_reloc_type )
     RELOC_NUMBER (R_RISCV_NONE,            0)   /* No reloc */
     RELOC_NUMBER (R_RISCV_64,              2)   /* Adjust by symbol value */
     RELOC_NUMBER (R_RISCV_RELATIVE,        3)   /* Adjust by object base */
     RELOC_NUMBER (R_RISCV_COPY,            4)   /* Copy symbol at runtime */
     RELOC_NUMBER (R_RISCV_JUMP_SLOT,       5)   /* Create PLT entry */
     RELOC_NUMBER (R_RISCV_TLS_DTPMOD64,    7)   /* Dynamic TLS module */
     RELOC_NUMBER (R_RISCV_TLS_DTPREL64,    9)   /* Dynamic TLS offset */
     RELOC_NUMBER (R_RISCV_TLS_TPREL64,    11)   /* Thread pointer relative */
     RELOC_NUMBER (R_RISCV_IRELATIVE,      58)   /* Indirect relative */
END_RELOC_NUMBERS (R_RISCV_max)

#endif
