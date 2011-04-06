/*	$OpenBSD: rtld_machine.c,v 1.15 2011/04/06 11:36:25 miod Exp $ */

/*
 * Copyright (c) 2004 Dale Rahn
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#define _DYN_LOADER
#define LDSO_ARCH_IS_RELA_

#include <sys/types.h>
#include <sys/mman.h>

#include <nlist.h>
#include <link.h>
#include <signal.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

void _dl_bind_start(void); /* XXX */
Elf_Addr _dl_bind(elf_object_t *object, int reloff);
#define _RF_S		0x80000000		/* Resolve symbol */
#define _RF_A		0x40000000		/* Use addend */
#define _RF_P		0x20000000		/* Location relative */
#define _RF_G		0x10000000		/* GOT offset */
#define _RF_B		0x08000000		/* Load address relative */
#define _RF_U		0x04000000		/* Unaligned */
#define _RF_E		0x02000000		/* ERROR */
#define _RF_SZ(s)	(((s) & 0xff) << 8)	/* memory target size */
#define _RF_RS(s)	((s) & 0xff)		/* right shift */
static int reloc_target_flags[] = {
	0,						/* 0	R_SH_NONE */
	_RF_S|_RF_A|            _RF_SZ(32) | _RF_RS(0), /* 1	R_SH_DIR32 */
	_RF_S|_RF_P|_RF_A|      _RF_SZ(32) | _RF_RS(0), /* 2  REL32 */
	_RF_E,						/* 3	R_SH_DIR8WPN */
	_RF_E,						/* 4	R_SH_IND12W */
	_RF_E,						/* 5	R_SH_DIR8WPL */
	_RF_E,						/* 6	R_SH_DIR8WPZ */
	_RF_E,						/* 7	R_SH_DIR8BP */
	_RF_E,						/* 8	R_SH_DIR8W */
	_RF_E,						/* 9	R_SH_DIR8L */
	_RF_E,						/* 10	R_SH_LOOP_START */
	_RF_E,						/* 11	R_SH_LOOP_END */
	_RF_E,						/* 12	Unused */
	_RF_E,						/* 13	Unused */
	_RF_E,						/* 14	Unused */
	_RF_E,						/* 15	Unused */
	_RF_E,						/* 16	Unused */
	_RF_E,						/* 17	Unused */
	_RF_E,						/* 18	Unused */
	_RF_E,						/* 19	Unused */
	_RF_E,						/* 20	Unused */
	_RF_E,						/* 21	Unused */
	_RF_E,						/* 22	R_SH_GNU_VTINHERIT */
	_RF_E,						/* 23	R_SH_GNU_VTENTRY */
	_RF_E,						/* 24	R_SH_SWITCH8 */
	_RF_E,						/* 25	R_SH_SWITCH16 */
	_RF_E,						/* 26	R_SH_SWITCH32 */
	_RF_E,						/* 27	R_SH_USES */
	_RF_E,						/* 28	R_SH_COUNT */
	_RF_E,						/* 29	R_SH_ALIGN */
	_RF_E,						/* 30	R_SH_CODE */
	_RF_E,						/* 31	R_SH_DATA */
	_RF_E,						/* 32	R_SH_LABEL */
	_RF_E,						/* 33	R_SH_DIR16 */
	_RF_E,						/* 34	R_SH_DIR8 */
	_RF_E,						/* 35	R_SH_DIR8UL */
	_RF_E,						/* 36	R_SH_DIR8UW */
	_RF_E,						/* 37	R_SH_DIR8U */
	_RF_E,						/* 38	R_SH_DIR8SW */
	_RF_E,						/* 39	R_SH_DIR8S */
	_RF_E,						/* 40	R_SH_DIR4UL */
	_RF_E,						/* 41	R_SH_DIR4UW */
	_RF_E,						/* 42	R_SH_DIR4U */
	_RF_E,						/* 43	R_SH_PSHA */
	_RF_E,						/* 44	R_SH_PSHL */
	_RF_E,						/* 45	R_SH_DIR5U */
	_RF_E,						/* 46	R_SH_DIR6U */
	_RF_E,						/* 47	R_SH_DIR6S */
	_RF_E,						/* 48	R_SH_DIR10S */
	_RF_E,						/* 49	R_SH_DIR10SW */
	_RF_E,						/* 50	R_SH_DIR10SL */
	_RF_E,						/* 51	R_SH_DIR10SQ */
	_RF_E,						/* 52	XXXX */
	_RF_E,						/* 53	R_SH_DIR16S */
	_RF_E,						/* 54	Unused */
	_RF_E,						/* 55	Unused */
	_RF_E,						/* 56	Unused */
	_RF_E,						/* 57	Unused */
	_RF_E,						/* 58	Unused */
	_RF_E,						/* 59	Unused */
	_RF_E,						/* 60	Unused */
	_RF_E,						/* 61	Unused */
	_RF_E,						/* 62	Unused */
	_RF_E,						/* 63	Unused */
	_RF_E,						/* 64	Unused */
	_RF_E,						/* 65	Unused */
	_RF_E,						/* 66	Unused */
	_RF_E,						/* 67	Unused */
	_RF_E,						/* 68	Unused */
	_RF_E,						/* 69	Unused */
	_RF_E,						/* 70	Unused */
	_RF_E,						/* 71	Unused */
	_RF_E,						/* 72	Unused */
	_RF_E,						/* 73	Unused */
	_RF_E,						/* 74	Unused */
	_RF_E,						/* 75	Unused */
	_RF_E,						/* 76	Unused */
	_RF_E,						/* 77	Unused */
	_RF_E,						/* 78	Unused */
	_RF_E,						/* 79	Unused */
	_RF_E,						/* 80	Unused */
	_RF_E,						/* 81	Unused */
	_RF_E,						/* 82	Unused */
	_RF_E,						/* 83	Unused */
	_RF_E,						/* 84	Unused */
	_RF_E,						/* 85	Unused */
	_RF_E,						/* 86	Unused */
	_RF_E,						/* 87	Unused */
	_RF_E,						/* 88	Unused */
	_RF_E,						/* 89	Unused */
	_RF_E,						/* 90	Unused */
	_RF_E,						/* 91	Unused */
	_RF_E,						/* 92	Unused */
	_RF_E,						/* 93	Unused */
	_RF_E,						/* 94	Unused */
	_RF_E,						/* 95	Unused */
	_RF_E,						/* 96	Unused */
	_RF_E,						/* 97	Unused */
	_RF_E,						/* 98	Unused */
	_RF_E,						/* 99	Unused */
	_RF_E,						/* 100	Unused */
	_RF_E,						/* 101	Unused */
	_RF_E,						/* 102	Unused */
	_RF_E,						/* 103	Unused */
	_RF_E,						/* 104	Unused */
	_RF_E,						/* 105	Unused */
	_RF_E,						/* 106	Unused */
	_RF_E,						/* 107	Unused */
	_RF_E,						/* 108	Unused */
	_RF_E,						/* 109	Unused */
	_RF_E,						/* 110	Unused */
	_RF_E,						/* 111	Unused */
	_RF_E,						/* 112	Unused */
	_RF_E,						/* 113	Unused */
	_RF_E,						/* 114	Unused */
	_RF_E,						/* 115	Unused */
	_RF_E,						/* 116	Unused */
	_RF_E,						/* 117	Unused */
	_RF_E,						/* 118	Unused */
	_RF_E,						/* 119	Unused */
	_RF_E,						/* 120	Unused */
	_RF_E,						/* 121	Unused */
	_RF_E,						/* 122	Unused */
	_RF_E,						/* 123	Unused */
	_RF_E,						/* 124	Unused */
	_RF_E,						/* 125	Unused */
	_RF_E,						/* 126	Unused */
	_RF_E,						/* 127	Unused */
	_RF_E,						/* 128	Unused */
	_RF_E,						/* 129	Unused */
	_RF_E,						/* 130	Unused */
	_RF_E,						/* 131	Unused */
	_RF_E,						/* 132	Unused */
	_RF_E,						/* 133	Unused */
	_RF_E,						/* 134	Unused */
	_RF_E,						/* 135	Unused */
	_RF_E,						/* 136	Unused */
	_RF_E,						/* 137	Unused */
	_RF_E,						/* 138	Unused */
	_RF_E,						/* 139	Unused */
	_RF_E,						/* 140	Unused */
	_RF_E,						/* 141	Unused */
	_RF_E,						/* 142	Unused */
	_RF_E,						/* 143	Unused */
	_RF_E,						/* 144	R_SH_TLS_GD_32 */
	_RF_E,						/* 145	R_SH_TLS_LD_32 */
	_RF_E,						/* 146	R_SH_TLS_LDO_32 */
	_RF_E,						/* 147	R_SH_TLS_IE_32 */
	_RF_E,						/* 148	R_SH_TLS_LE_32 */
	_RF_E,						/* 149	R_SH_TLS_DTPMOD32 */
	_RF_E,						/* 150	R_SH_TLS_DTPOFF32 */
	_RF_E,						/* 151	R_SH_TLS_TPOFF32 */
	_RF_E,						/* 152 Unused */
	_RF_E,						/* 153 Unused */
	_RF_E,						/* 154 Unused */
	_RF_E,						/* 155 Unused */
	_RF_E,						/* 156 Unused */
	_RF_E,						/* 157 Unused */
	_RF_E,						/* 158 Unused */
	_RF_E,						/* 159 Unused */
	_RF_E,						/* 160	R_SH_GOT32 */
	_RF_E,						/* 161	R_SH_PLT32 */
	_RF_S|			_RF_SZ(32) | _RF_RS(0),	/* 162	COPY */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),	/* 163	GLOB_DAT */
	_RF_S|			_RF_SZ(32) | _RF_RS(0),	/* 164	JMP_SLOT */
	      _RF_A|	_RF_B|	_RF_SZ(32) | _RF_RS(0),	/* 165 RELATIVE */
	_RF_E,						/* 166	R_SH_GOTOFF */
	_RF_E,						/* 167	R_SH_GOTPC */
	_RF_E,						/* 168	R_SH_GOTPLT32 */
	_RF_E,						/* 169	R_SH_GOT_LOW16 */
	_RF_E,						/* 170	R_SH_GOT_MEDLOW16 */
	_RF_E,						/* 171	R_SH_GOT_MEDHI16 */
	_RF_E,						/* 172	R_SH_GOT_HI16 */
	_RF_E,						/* 173	R_SH_GOTPLT_LOW16 */
	_RF_E,						/* 174	R_SH_GOTPLT_MEDLOW16 */
	_RF_E,						/* 175	R_SH_GOTPLT_MEDHI16 */
	_RF_E,						/* 176	R_SH_GOTPLT_HI16 */
	_RF_E,						/* 177	R_SH_PLT_LOW16 */
	_RF_E,						/* 178	R_SH_PLT_MEDLOW16 */
	_RF_E,						/* 179	R_SH_PLT_MEDHI16 */
	_RF_E,						/* 180	R_SH_PLT_HI16 */
	_RF_E,						/* 181	R_SH_GOTOFF_LOW16 */
	_RF_E,						/* 182	R_SH_GOTOFF_MEDLOW16 */
	_RF_E,						/* 183	R_SH_GOTOFF_MEDHI16 */
	_RF_E,						/* 184	R_SH_GOTOFF_HI16 */
	_RF_E,						/* 185	R_SH_GOTPC_LOW16 */
	_RF_E,						/* 186	R_SH_GOTPC_MEDLOW16 */
	_RF_E,						/* 187	R_SH_GOTPC_MEDHI16 */
	_RF_E,						/* 188	R_SH_GOTPC_HI16 */
	_RF_E,						/* 189	R_SH_GOT10BY4 */
	_RF_E,						/* 190	R_SH_GOTPLT10BY4 */
	_RF_E,						/* 191	R_SH_GOT10BY8 */
	_RF_E,						/* 192	R_SH_GOTPLT10BY8 */
#ifdef SH_SUPPORT_64_BIT
	_RF_E,						/* 193	R_SH_COPY64 */
	_RF_E,						/* 194	R_SH_GLOB_DAT64 */
	_RF_E,						/* 195	R_SH_JMP_SLOT64 */
	_RF_E,						/* 196	R_SH_RELATIVE64 */
	_RF_E,						/* 197	Unused */
	_RF_E,						/* 198	Unused */
	_RF_E,						/* 199	Unused */
	_RF_E,						/* 200	Unused */
	_RF_E,						/* 201	Unused */
	_RF_E,						/* 202	Unused */
	_RF_E,						/* 203	Unused */
	_RF_E,						/* 204	Unused */
	_RF_E,						/* 205	Unused */
	_RF_E,						/* 206	Unused */
	_RF_E,						/* 207	Unused */
	_RF_E,						/* 208	Unused */
	_RF_E,						/* 209	Unused */
	_RF_E,						/* 210	Unused */
	_RF_E,						/* 211	Unused */
	_RF_E,						/* 212	Unused */
	_RF_E,						/* 213	Unused */
	_RF_E,						/* 214	Unused */
	_RF_E,						/* 215	Unused */
	_RF_E,						/* 216	Unused */
	_RF_E,						/* 217	Unused */
	_RF_E,						/* 218	Unused */
	_RF_E,						/* 219	Unused */
	_RF_E,						/* 220	Unused */
	_RF_E,						/* 221	Unused */
	_RF_E,						/* 222	Unused */
	_RF_E,						/* 223	Unused */
	_RF_E,						/* 224	Unused */
	_RF_E,						/* 225	Unused */
	_RF_E,						/* 226	Unused */
	_RF_E,						/* 227	Unused */
	_RF_E,						/* 228	Unused */
	_RF_E,						/* 229	Unused */
	_RF_E,						/* 230	Unused */
	_RF_E,						/* 231	Unused */
	_RF_E,						/* 232	Unused */
	_RF_E,						/* 233	Unused */
	_RF_E,						/* 234	Unused */
	_RF_E,						/* 235	Unused */
	_RF_E,						/* 236	Unused */
	_RF_E,						/* 237	Unused */
	_RF_E,						/* 238	Unused */
	_RF_E,						/* 239	Unused */
	_RF_E,						/* 240	Unused */
	_RF_E,						/* 241	Unused */
	_RF_E,						/* 242	R_SH_SHMEDIA_CODE */
	_RF_E,						/* 243	R_SH_PT_16 */
	_RF_E,						/* 244	R_SH_IMMS16 */
	_RF_E,						/* 245	R_SH_IMMU16 */
	_RF_E,						/* 246	R_SH_IMM_LOW16 */
	_RF_E,						/* 247	R_SH_IMM_LOW16_PCREL */
	_RF_E,						/* 248	R_SH_IMM_MEDLOW16 */
	_RF_E,						/* 249	R_SH_IMM_MEDLOW16_PCREL */
	_RF_E,						/* 250	R_SH_IMM_MEDHI16 */
	_RF_E,						/* 251	R_SH_IMM_MEDHI16_PCREL */
	_RF_E,						/* 252	R_SH_IMM_HI16 */
	_RF_E,						/* 253	R_SH_IMM_HI16_PCREL */
	_RF_E,						/* 254	R_SH_64 */
	_RF_E,						/* 255	R_SH_64_PCREL */
	0
#endif
};

#define RELOC_RESOLVE_SYMBOL(t)		((reloc_target_flags[t] & _RF_S) != 0)
#define RELOC_PC_RELATIVE(t)		((reloc_target_flags[t] & _RF_P) != 0)
#define RELOC_BASE_RELATIVE(t)		((reloc_target_flags[t] & _RF_B) != 0)
#define RELOC_UNALIGNED(t)		((reloc_target_flags[t] & _RF_U) != 0)
#define RELOC_USE_ADDEND(t)		((reloc_target_flags[t] & _RF_A) != 0)
#define RELOC_TARGET_SIZE(t)		((reloc_target_flags[t] >> 8) & 0xff)
#define RELOC_VALUE_RIGHTSHIFT(t)	(reloc_target_flags[t] & 0xff)
static int reloc_target_bitmask[] = {
#define _BM(x)  (x == 32? ~0 : ~(-(1UL << (x))))
	_BM(0),		/* 0	R_SH_NONE */
	_BM(32),	/* 1	R_SH_DIR32 */
	_BM(32),	/* 2	R_SH_REL32 */
	_BM(8),		/* 3	R_SH_DIR8WPN */
	_BM(12),	/* 4	R_SH_IND12W */
	_BM(8),		/* 5	R_SH_DIR8WPL */
	_BM(8),		/* 6	R_SH_DIR8WPZ */
	_BM(8),		/* 7	R_SH_DIR8BP */
	_BM(8),		/* 8	R_SH_DIR8W */
	_BM(8),		/* 9	R_SH_DIR8L */
	_BM(0),		/* 10	R_SH_LOOP_START */
	_BM(0),		/* 11	R_SH_LOOP_END */
	_BM(0),		/* 12	Unused */
	_BM(0),		/* 13	Unused */
	_BM(0),		/* 14	Unused */
	_BM(0),		/* 15	Unused */
	_BM(0),		/* 16	Unused */
	_BM(0),		/* 17	Unused */
	_BM(0),		/* 18	Unused */
	_BM(0),		/* 19	Unused */
	_BM(0),		/* 20	Unused */
	_BM(0),		/* 21	Unused */
	_BM(0),		/* 22	R_SH_GNU_VTINHERIT */
	_BM(0),		/* 23	R_SH_GNU_VTENTRY */
	_BM(0),		/* 24	R_SH_SWITCH8 */
	_BM(0),		/* 25	R_SH_SWITCH16 */
	_BM(0),		/* 26	R_SH_SWITCH32 */
	_BM(0),		/* 27	R_SH_USES */
	_BM(0),		/* 28	R_SH_COUNT */
	_BM(0),		/* 29	R_SH_ALIGN */
	_BM(0),		/* 30	R_SH_CODE */
	_BM(0),		/* 31	R_SH_DATA */
	_BM(0),		/* 32	R_SH_LABEL */
	_BM(0),		/* 33	R_SH_DIR16 */
	_BM(0),		/* 34	R_SH_DIR8 */
	_BM(0),		/* 35	R_SH_DIR8UL */
	_BM(0),		/* 36	R_SH_DIR8UW */
	_BM(0),		/* 37	R_SH_DIR8U */
	_BM(0),		/* 38	R_SH_DIR8SW */
	_BM(0),		/* 39	R_SH_DIR8S */
	_BM(0),		/* 40	R_SH_DIR4UL */
	_BM(0),		/* 41	R_SH_DIR4UW */
	_BM(0),		/* 42	R_SH_DIR4U */
	_BM(0),		/* 43	R_SH_PSHA */
	_BM(0),		/* 44	R_SH_PSHL */
	_BM(0),		/* 45	R_SH_DIR5U */
	_BM(0),		/* 46	R_SH_DIR6U */
	_BM(0),		/* 47	R_SH_DIR6S */
	_BM(0),		/* 48	R_SH_DIR10S */
	_BM(0),		/* 49	R_SH_DIR10SW */
	_BM(0),		/* 50	R_SH_DIR10SL */
	_BM(0),		/* 51	R_SH_DIR10SQ */
	_BM(0),		/* 52	xxx */
	_BM(0),		/* 53	R_SH_DIR16S */
	_BM(0),		/* 54	Unused */
	_BM(0),		/* 55	Unused */
	_BM(0),		/* 56	Unused */
	_BM(0),		/* 57	Unused */
	_BM(0),		/* 58	Unused */
	_BM(0),		/* 59	Unused */
	_BM(0),		/* 60	Unused */
	_BM(0),		/* 61	Unused */
	_BM(0),		/* 62	Unused */
	_BM(0),		/* 63	Unused */
	_BM(0),		/* 64	Unused */
	_BM(0),		/* 65	Unused */
	_BM(0),		/* 66	Unused */
	_BM(0),		/* 67	Unused */
	_BM(0),		/* 68	Unused */
	_BM(0),		/* 69	Unused */
	_BM(0),		/* 70	Unused */
	_BM(0),		/* 71	Unused */
	_BM(0),		/* 72	Unused */
	_BM(0),		/* 73	Unused */
	_BM(0),		/* 74	Unused */
	_BM(0),		/* 75	Unused */
	_BM(0),		/* 76	Unused */
	_BM(0),		/* 77	Unused */
	_BM(0),		/* 78	Unused */
	_BM(0),		/* 79	Unused */
	_BM(0),		/* 80	Unused */
	_BM(0),		/* 81	Unused */
	_BM(0),		/* 82	Unused */
	_BM(0),		/* 83	Unused */
	_BM(0),		/* 84	Unused */
	_BM(0),		/* 85	Unused */
	_BM(0),		/* 86	Unused */
	_BM(0),		/* 87	Unused */
	_BM(0),		/* 88	Unused */
	_BM(0),		/* 89	Unused */
	_BM(0),		/* 90	Unused */
	_BM(0),		/* 91	Unused */
	_BM(0),		/* 92	Unused */
	_BM(0),		/* 93	Unused */
	_BM(0),		/* 94	Unused */
	_BM(0),		/* 95	Unused */
	_BM(0),		/* 96	Unused */
	_BM(0),		/* 97	Unused */
	_BM(0),		/* 98	Unused */
	_BM(0),		/* 99	Unused */
	_BM(0),		/* 100	Unused */
	_BM(0),		/* 101	Unused */
	_BM(0),		/* 102	Unused */
	_BM(0),		/* 103	Unused */
	_BM(0),		/* 104	Unused */
	_BM(0),		/* 105	Unused */
	_BM(0),		/* 106	Unused */
	_BM(0),		/* 107	Unused */
	_BM(0),		/* 108	Unused */
	_BM(0),		/* 109	Unused */
	_BM(0),		/* 110	Unused */
	_BM(0),		/* 111	Unused */
	_BM(0),		/* 112	Unused */
	_BM(0),		/* 113	Unused */
	_BM(0),		/* 114	Unused */
	_BM(0),		/* 115	Unused */
	_BM(0),		/* 116	Unused */
	_BM(0),		/* 117	Unused */
	_BM(0),		/* 118	Unused */
	_BM(0),		/* 119	Unused */
	_BM(0),		/* 120	Unused */
	_BM(0),		/* 121	Unused */
	_BM(0),		/* 122	Unused */
	_BM(0),		/* 123	Unused */
	_BM(0),		/* 124	Unused */
	_BM(0),		/* 125	Unused */
	_BM(0),		/* 126	Unused */
	_BM(0),		/* 127	Unused */
	_BM(0),		/* 128	Unused */
	_BM(0),		/* 129	Unused */
	_BM(0),		/* 130	Unused */
	_BM(0),		/* 131	Unused */
	_BM(0),		/* 132	Unused */
	_BM(0),		/* 133	Unused */
	_BM(0),		/* 134	Unused */
	_BM(0),		/* 135	Unused */
	_BM(0),		/* 136	Unused */
	_BM(0),		/* 137	Unused */
	_BM(0),		/* 138	Unused */
	_BM(0),		/* 139	Unused */
	_BM(0),		/* 140	Unused */
	_BM(0),		/* 141	Unused */
	_BM(0),		/* 142	Unused */
	_BM(0),		/* 143	Unused */
	_BM(0),		/* 144	R_SH_TLS_GD_32 */
	_BM(0),		/* 145	R_SH_TLS_LD_32 */
	_BM(0),		/* 146	R_SH_TLS_LDO_32 */
	_BM(0),		/* 147	R_SH_TLS_IE_32 */
	_BM(0),		/* 148	R_SH_TLS_LE_32 */
	_BM(0),		/* 149	R_SH_TLS_DTPMOD32 */
	_BM(0),		/* 150	R_SH_TLS_DTPOFF32 */
	_BM(0),		/* 151	R_SH_TLS_TPOFF32 */
	_BM(0),		/* 152  xxx */
	_BM(0),		/* 153  xxx */
	_BM(0),		/* 154  xxx */
	_BM(0),		/* 155  xxx */
	_BM(0),		/* 156  xxx */
	_BM(0),		/* 157  xxx */
	_BM(0),		/* 158  xxx */
	_BM(0),		/* 159  xxx */
	_BM(0),		/* 160	R_SH_GOT32 */
	_BM(0),		/* 161	R_SH_PLT32 */
	_BM(0),		/* 162	R_SH_COPY */
	_BM(32),	/* 163	R_SH_GLOB_DAT */
	_BM(0),		/* 164	R_SH_JMP_SLOT */
	_BM(32),	/* 165	R_SH_RELATIVE */
	_BM(0),		/* 166	R_SH_GOTOFF */
	_BM(0),		/* 167	R_SH_GOTPC */
	_BM(0),		/* 168	R_SH_GOTPLT32 */
	_BM(0),		/* 169	R_SH_GOT_LOW16 */
	_BM(0),		/* 170	R_SH_GOT_MEDLOW16 */
	_BM(0),		/* 171	R_SH_GOT_MEDHI16 */
	_BM(0),		/* 172	R_SH_GOT_HI16 */
	_BM(0),		/* 173	R_SH_GOTPLT_LOW16 */
	_BM(0),		/* 174	R_SH_GOTPLT_MEDLOW16 */
	_BM(0),		/* 175	R_SH_GOTPLT_MEDHI16 */
	_BM(0),		/* 176	R_SH_GOTPLT_HI16 */
	_BM(0),		/* 177	R_SH_PLT_LOW16 */
	_BM(0),		/* 178	R_SH_PLT_MEDLOW16 */
	_BM(0),		/* 179	R_SH_PLT_MEDHI16 */
	_BM(0),		/* 180	R_SH_PLT_HI16 */
	_BM(0),		/* 181	R_SH_GOTOFF_LOW16 */
	_BM(0),		/* 182	R_SH_GOTOFF_MEDLOW16 */
	_BM(0),		/* 183	R_SH_GOTOFF_MEDHI16 */
	_BM(0),		/* 184	R_SH_GOTOFF_HI16 */
	_BM(0),		/* 185	R_SH_GOTPC_LOW16 */
	_BM(0),		/* 186	R_SH_GOTPC_MEDLOW16 */
	_BM(0),		/* 187	R_SH_GOTPC_MEDHI16 */
	_BM(0),		/* 188	R_SH_GOTPC_HI16 */
	_BM(0),		/* 189	R_SH_GOT10BY4 */
	_BM(0),		/* 190	R_SH_GOTPLT10BY4 */
	_BM(0),		/* 191	R_SH_GOT10BY8 */
	_BM(0),		/* 192	R_SH_GOTPLT10BY8 */
#ifdef SH_SUPPORT_64_BIT
	_BM(0),		/* 193	R_SH_COPY64 */
	_BM(0),		/* 194	R_SH_GLOB_DAT64 */
	_BM(0),		/* 195	R_SH_JMP_SLOT64 */
	_BM(0),		/* 196	R_SH_RELATIVE64 */
	_BM(0),		/* 197 xxx */
	_BM(0),		/* 198 xxx */
	_BM(0),		/* 199 xxx */
	_BM(0),		/* 200 xxx */
	_BM(0),		/* 201 xxx */
	_BM(0),		/* 202 xxx */
	_BM(0),		/* 203 xxx */
	_BM(0),		/* 204 xxx */
	_BM(0),		/* 205 xxx */
	_BM(0),		/* 206 xxx */
	_BM(0),		/* 207 xxx */
	_BM(0),		/* 208 xxx */
	_BM(0),		/* 209 xxx */
	_BM(0),		/* 210 xxx */
	_BM(0),		/* 211 xxx */
	_BM(0),		/* 212 xxx */
	_BM(0),		/* 213 xxx */
	_BM(0),		/* 214 xxx */
	_BM(0),		/* 215 xxx */
	_BM(0),		/* 216 xxx */
	_BM(0),		/* 217 xxx */
	_BM(0),		/* 218 xxx */
	_BM(0),		/* 219 xxx */
	_BM(0),		/* 220 xxx */
	_BM(0),		/* 221 xxx */
	_BM(0),		/* 222 xxx */
	_BM(0),		/* 223 xxx */
	_BM(0),		/* 224 xxx */
	_BM(0),		/* 225 xxx */
	_BM(0),		/* 226 xxx */
	_BM(0),		/* 227 xxx */
	_BM(0),		/* 228 xxx */
	_BM(0),		/* 229  xxx */
	_BM(0),		/* 230 xxx */
	_BM(0),		/* 231 xxx */
	_BM(0),		/* 232 xxx */
	_BM(0),		/* 233 xxx */
	_BM(0),		/* 234 xxx */
	_BM(0),		/* 235 xxx */
	_BM(0),		/* 236 xxx */
	_BM(0),		/* 237 xxx */
	_BM(0),		/* 238 xxx */
	_BM(0),		/* 239 xxx */
	_BM(0),		/* 240 xxx */
	_BM(0),		/* 241 xxx */
	_BM(0),		/* 242	R_SH_SHMEDIA_CODE */
	_BM(0),		/* 243	R_SH_PT_16 */
	_BM(0),		/* 244	R_SH_IMMS16 */
	_BM(0),		/* 245	R_SH_IMMU16 */
	_BM(0),		/* 246	R_SH_IMM_LOW16 */
	_BM(0),		/* 247	R_SH_IMM_LOW16_PCREL */
	_BM(0),		/* 248	R_SH_IMM_MEDLOW16 */
	_BM(0),		/* 249	R_SH_IMM_MEDLOW16_PCREL */
	_BM(0),		/* 250	R_SH_IMM_MEDHI16 */
	_BM(0),		/* 251	R_SH_IMM_MEDHI16_PCREL */
	_BM(0),		/* 252	R_SH_IMM_HI16 */
	_BM(0),		/* 253	R_SH_IMM_HI16_PCREL */
	_BM(0),		/* 254	R_SH_64 */
	_BM(0),		/* 255	R_SH_64_PCREL */
#endif
#undef _BM
};
#define RELOC_VALUE_BITMASK(t)	(reloc_target_bitmask[t])

#define R_TYPE(x) R_SH_ ## x

void _dl_reloc_plt(Elf_Word *where, Elf_Addr value, Elf_RelA *rel);

void
_dl_reloc_plt(Elf_Word *where, Elf_Addr value, Elf_RelA *rel)
{
	*where = value + rel->r_addend;
}

int
_dl_md_reloc(elf_object_t *object, int rel, int relasz)
{
	long	i;
	long	numrela;
	int	fails = 0;
	Elf_Addr loff;
	Elf_RelA *rels;
	struct load_list *llist;

	loff = object->obj_base;
	numrela = object->Dyn.info[relasz] / sizeof(Elf_RelA);
	rels = (Elf_RelA *)(object->Dyn.info[rel]);

	if (rels == NULL)
		return(0);

	/*
	 * unprotect some segments if we need it.
	 */
	if ((object->dyn.textrel == 1) && (rel == DT_REL || rel == DT_RELA)) {
		for (llist = object->load_list;
		    llist != NULL;
		    llist = llist->next) {
			if (!(llist->prot & PROT_WRITE))
				_dl_mprotect(llist->start, llist->size,
				    llist->prot|PROT_WRITE);
		}
	}

	for (i = 0; i < numrela; i++, rels++) {
		Elf_Addr *where, value, ooff, mask;
		Elf_Word type;
		const Elf_Sym *sym, *this;
		const char *symn;

		type = ELF_R_TYPE(rels->r_info);

		if (reloc_target_flags[type] & _RF_E) {
			_dl_printf(" bad relocation obj %s %d %d\n", object->load_name, i, type);
			_dl_exit(1);
		}
		if (type == R_TYPE(NONE))
			continue;

		if (type == R_TYPE(JMP_SLOT) && rel != DT_JMPREL)
			continue;

		where = (Elf_Addr *)(rels->r_offset + loff);

		if (RELOC_USE_ADDEND(type))
#ifdef LDSO_ARCH_IS_RELA_
			value = rels->r_addend;
#else
			value = *where & RELOC_VALUE_BITMASK(type);
#endif
		else
			value = 0;


		sym = NULL;
		symn = NULL;
		if (RELOC_RESOLVE_SYMBOL(type)) {
			sym = object->dyn.symtab;
			sym += ELF_R_SYM(rels->r_info);
			symn = object->dyn.strtab + sym->st_name;

			if (sym->st_shndx != SHN_UNDEF &&
			    ELF_ST_BIND(sym->st_info) == STB_LOCAL) {
				value += loff;
			} else {
				this = NULL;
#if 1
				ooff = _dl_find_symbol_bysym(object,
				    ELF_R_SYM(rels->r_info), &this,
				    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|
				    ((type == R_TYPE(JMP_SLOT)) ?
					SYM_PLT : SYM_NOTPLT),
				    sym, NULL);
#else
				ooff = _dl_find_symbol_bysym(object,
				    ELF_R_SYM(rels->r_info), &this,
				    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|
				    SYM_PLT,
				    sym, NULL);
#endif
				if (this == NULL) {
resolve_failed:
					if (ELF_ST_BIND(sym->st_info) !=
					    STB_WEAK)
						fails++;
					continue;
				}
				value += (Elf_Addr)(ooff + this->st_value);
			}
		}

		if (type == R_TYPE(JMP_SLOT)) {
			_dl_reloc_plt((Elf_Word *)where, value, rels);
			continue;
		}

		if (type == R_TYPE(COPY)) {
			void *dstaddr = where;
			const void *srcaddr;
			const Elf_Sym *dstsym = sym, *srcsym = NULL;
			Elf_Addr soff;

			soff = _dl_find_symbol(symn, &srcsym,
			    SYM_SEARCH_OTHER|SYM_WARNNOTFOUND|SYM_NOTPLT,
			    dstsym, object, NULL);
			if (srcsym == NULL)
				goto resolve_failed;

			srcaddr = (void *)(soff + srcsym->st_value);
			_dl_bcopy(srcaddr, dstaddr, dstsym->st_size);
			continue;
		}

		if (RELOC_PC_RELATIVE(type))
			value -= (Elf_Addr)where;
		if (RELOC_BASE_RELATIVE(type))
			value += loff;

		mask = RELOC_VALUE_BITMASK(type);
		value >>= RELOC_VALUE_RIGHTSHIFT(type);
		value &= mask;

		if (RELOC_UNALIGNED(type)) {
			/* Handle unaligned relocations. */
			Elf_Addr tmp = 0;
			char *ptr = (char *)where;
			int i, size = RELOC_TARGET_SIZE(type)/8;

			/* Read it in one byte at a time. */
			for (i=0; i<size; i++)
				tmp = (tmp << 8) | ptr[i];

			tmp &= ~mask;
			tmp |= value;

			/* Write it back out. */
			for (i=0; i<size; i++)
				ptr[i] = ((tmp >> (8*i)) & 0xff);
		} else {
			*where &= ~mask;
			*where |= value;
		}
	}

	/* reprotect the unprotected segments */
	if ((object->dyn.textrel == 1) && (rel == DT_REL || rel == DT_RELA)) {
		for (llist = object->load_list;
		    llist != NULL;
		    llist = llist->next) {
			if (!(llist->prot & PROT_WRITE))
				_dl_mprotect(llist->start, llist->size,
				    llist->prot);
		}
	}

	return (fails);
}

/*
 *	Relocate the Global Offset Table (GOT).
 *	This is done by calling _dl_md_reloc on DT_JUMPREL for DL_BIND_NOW,
 *	otherwise the lazy binding plt initialization is performed.
 */
int
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	int	fails = 0;
	Elf_Addr *pltgot = (Elf_Addr *)object->Dyn.info[DT_PLTGOT];
	Elf_Addr ooff;
	const Elf_Sym *this;
	int i, num;
	Elf_Rel *rel;

	/* XXX - lazy binding not supported yet */
	lazy = 0;

	if (object->Dyn.info[DT_PLTREL] != DT_RELA)
		return (0);

	object->got_addr = 0;
	object->got_size = 0;
	this = NULL;
	ooff = _dl_find_symbol("__got_start", &this,
	    SYM_SEARCH_OBJ|SYM_NOWARNNOTFOUND|SYM_PLT, NULL, object, NULL);
	if (this != NULL)
		object->got_addr = ooff + this->st_value;

	this = NULL;
	ooff = _dl_find_symbol("__got_end", &this,
	    SYM_SEARCH_OBJ|SYM_NOWARNNOTFOUND|SYM_PLT, NULL, object, NULL);
	if (this != NULL)
		object->got_size = ooff + this->st_value  - object->got_addr;

	object->plt_size = 0;	/* Text PLT on ARM */

	if (object->got_addr == 0)
		object->got_start = 0;
	else {
		object->got_start = ELF_TRUNC(object->got_addr, _dl_pagesz);
		object->got_size += object->got_addr - object->got_start;
		object->got_size = ELF_ROUND(object->got_size, _dl_pagesz);
	}
	object->plt_start = 0;

	if (!lazy) {
		fails = _dl_md_reloc(object, DT_JMPREL, DT_PLTRELSZ);
	} else {
		rel = (Elf_Rel *)(object->Dyn.info[DT_JMPREL]);
		num = (object->Dyn.info[DT_PLTRELSZ]);

		for (i = 0; i < num/sizeof(Elf_Rel); i++, rel++) {
			Elf_Addr *where;
			where = (Elf_Addr *)(rel->r_offset + object->obj_base);
			*where += object->obj_base;
		}

		pltgot[1] = (Elf_Addr)object;
		pltgot[2] = (Elf_Addr)_dl_bind_start;
	}
	if (object->got_size != 0)
		_dl_mprotect((void*)object->got_start, object->got_size,
		    PROT_READ);
	if (object->plt_size != 0)
		_dl_mprotect((void*)object->plt_start, object->plt_size,
		    PROT_READ|PROT_EXEC);

	return (fails);
}

Elf_Addr
_dl_bind(elf_object_t *object, int relidx)
{
	Elf_Rel *rel;
	Elf_Word *addr;
	const Elf_Sym *sym, *this;
	const char *symn;
	Elf_Addr ooff, newval;
	sigset_t savedmask;

	rel = ((Elf_Rel *)object->Dyn.info[DT_JMPREL]) + (relidx);

	sym = object->dyn.symtab;
	sym += ELF_R_SYM(rel->r_info);
	symn = object->dyn.strtab + sym->st_name;

	this = NULL;
	ooff = _dl_find_symbol(symn,  &this,
	    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT, sym,
	    object, NULL);
	if (this == NULL) {
		_dl_printf("lazy binding failed!\n");
		*((int *)0) = 0;	/* XXX */
	}

	addr = (Elf_Addr *)(object->obj_base + rel->r_offset);
	newval = ooff + this->st_value;

	/* if GOT is protected, allow the write */
	if (object->got_size != 0) {
		_dl_thread_bind_lock(0, &savedmask);
		_dl_mprotect((void*)object->got_start, object->got_size,
		    PROT_READ|PROT_WRITE);
	}

	if (*addr != newval)
		*addr = newval;

	/* put the GOT back to RO */
	if (object->got_size != 0) {
		_dl_mprotect((void*)object->got_start, object->got_size,
		    PROT_READ);
		_dl_thread_bind_lock(1, &savedmask);
	}
	return newval;
}
