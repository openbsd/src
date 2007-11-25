/*
 * Copyright (c) 2007 Gordon Willem Klok <gwk@openbsd.org>
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

/*
 * Information taken from VESA Bios Extention (VBE) Core Functions Standard
 * Version 3.0 found at http://www.vesa.org/public/VBE/vbe3.pdf
 */
#ifndef VBE_H
#define VBE_H

#define BIOS_VIDEO_INTR			0x10

/* A well know address to locate a page at in the vm86 task */
#define KVM86_CALL_TASKVA		0x2000

/* Information contained in AH following a VBE function call */
/* Low byte determines call support */
#define VBECALL_SUPPORT(v) 		(v & 0xff)
#define VBECALL_SUPPORTED		0x4f

/* High byte determines call success */
#define VBECALL_SUCCESS(v)		(v >> 8 & 0xFF)
#define VBECALL_SUCCEDED		0x00
#define VBECALL_FAILED			0x01
#define VBECALL_MISMATCH		0x02 /* BIOS SUPPORTS HW DOES NOT */
#define VBECALL_INVALID			0x03 /* INVALID IN CURRENT MODE */

/* VBE Standard Function Calls */
#define VBE_FUNC_CTRLINFO		0x4F00
#define VBE_FUNC_MODEINFO		0x4F01
#define VBE_FUNC_SETMODE		0x4F02
#define VBE_FUNC_GETMODE		0x4F03
#define VBE_FUNC_SAVEREST		0x4F04
#define VBE_FUNC_DWC			0x4F05 /* Display window control */
#define VBE_FUNC_LSLL			0x4F06 /* Logical Scan Line Length */
#define VBE_FUNC_START			0x4F07 /* Set/Get Display Start */
#define VBE_FUNC_DAC			0x4F08 /* Set/Get DAC Pallete Format */
#define VBE_FUNC_PALETTE		0x4F09 /* Set/Get Pallete Data */
#define VBE_FUNC_PMI			0x4F0A /* Protected Mode Interface */
#define VBE_FUNC_PIXELCLOCK		0x4F0B

/* VBE Supplemental Function Calls */
#define VBE_FUNC_PM			0x4F10 /* Power Management Interface */
#define VBE_FUNC_FLATPANEL		0x4F11 /* Flat Panel Interface */
#define VBE_FUNC_AUDIO			0x4F13 /* Audio Interface */
#define VBE_FUNC_OEM			0x4F14 /* OEM Extentions */
#define VBE_FUNC_DDC			0x4F15 /* Display Data Channel (DDC) */

#define VBE_CTRLINFO_VERSION(v)		(v >> 8)
#define VBE_CTRLINFO_REVISION(v)	(v & 0xff)
#define VBE_DDC_GET			0x01

struct edid_chroma {
	uint8_t chroma_rglow;
	uint8_t chroma_bwlow;
	uint8_t chroma_redx;
	uint8_t chroma_redy;
	uint8_t chroma_greenx;
	uint8_t chroma_greeny;
	uint8_t chroma_bluex;
	uint8_t chroma_bluey;
	uint8_t chroma_whitex;
	uint8_t chroma_whitey;
} __packed;

struct edid_db {
	uint16_t db_pixelclock;
	uint8_t db_stor[16];
} __packed;

#define EDID_DB_FLAG_INTERLACED		0x80
#define EDID_DB_FLAG_STEREO		0x10
#define EDID_DB_POSITIVE_HSYNC		0x04
#define EDID_DB_POSITIVE_VSYNC		0x02

/* Types of Descriptor Blocks */
#define EDID_DB_BT_MONSERIAL		0xff
#define EDID_DB_BT_ASCIISTR		0xfe
#define EDID_DB_BT_RANGELIMITS		0xfd
#define EDID_DB_BT_MONITORNAME		0xfc
#define EDID_DB_BT_COLORPOINT		0xfb
#define EDID_DB_BT_STDTIMEDATA		0xfa
#define EDID_DB_BT_UNDEF		0xf9
#define EDID_DB_BT_MANUFDEF		0xf8

struct edid_ranges {
	uint8_t range_minvertfreq;
	uint8_t range_maxvertfreq;
	uint8_t range_minhorizfreq;
	uint8_t range_maxhorizfreq;
	uint8_t range_pixelclock;
	uint16_t range_secgtftoggle;
	uint8_t range_starthorizfreq;
	uint8_t range_c;
	uint16_t range_m;
	uint16_t range_k;
	uint16_t range_y;
} __packed;

struct edid_timing {
	uint8_t timing_estb1;
	uint8_t timing_estb2;
	uint8_t timing_manu;
	uint16_t timing_std1;
	uint16_t timing_std2;
	uint16_t timing_std3;
	uint16_t timing_std4;
	uint16_t timing_std5;
	uint16_t timing_std6;
	uint16_t timing_std7;
	uint16_t timing_std8;
} __packed;

struct edid {
	uint8_t edid_header[8];
	uint16_t edid_manufacture;
	uint16_t edid_product;
	uint32_t edid_serialno;
	uint8_t edid_week;
	uint8_t edid_year;
	uint8_t edid_version;
	uint8_t edid_revision;
	uint8_t edid_vidid;
	uint8_t edid_maxhoriz_isize;
	uint8_t edid_maxvert_isize;
	uint8_t edid_gamma;
	uint8_t edid_pmfeatures;
	struct edid_chroma edid_chroma;
	struct edid_timing edid_timing;
	struct edid_db edid_db1;
	struct edid_db edid_db2;
	struct edid_db edid_db3;
	struct edid_db edid_db4;
	uint8_t edid_extblocks;
	uint8_t edid_chksum;
} __packed;

#endif
