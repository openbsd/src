/*	$OpenBSD: sdmmcreg.h,v 1.4 2009/01/09 10:55:22 jsg Exp $	*/

/*
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
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

#ifndef _SDMMCREG_H_
#define _SDMMCREG_H_

/* MMC commands */				/* response type */
#define MMC_GO_IDLE_STATE		0	/* R0 */
#define MMC_SEND_OP_COND		1	/* R3 */
#define MMC_ALL_SEND_CID		2	/* R2 */
#define MMC_SET_RELATIVE_ADDR   	3	/* R1 */
#define MMC_SELECT_CARD			7	/* R1 */
#define MMC_SEND_CSD			9	/* R2 */
#define MMC_STOP_TRANSMISSION		12	/* R1B */
#define MMC_SEND_STATUS			13	/* R1 */
#define MMC_SET_BLOCKLEN		16	/* R1 */
#define MMC_READ_BLOCK_SINGLE		17	/* R1 */
#define MMC_READ_BLOCK_MULTIPLE		18	/* R1 */
#define MMC_SET_BLOCK_COUNT		23	/* R1 */
#define MMC_WRITE_BLOCK_SINGLE		24	/* R1 */
#define MMC_WRITE_BLOCK_MULTIPLE	25	/* R1 */
#define MMC_APP_CMD			55	/* R1 */

/* SD commands */				/* response type */
#define SD_SEND_RELATIVE_ADDR		3	/* R6 */
#define SD_SEND_IF_COND			8	/* R7 */

/* SD application commands */			/* response type */
#define SD_APP_SET_BUS_WIDTH		6	/* R1 */
#define SD_APP_OP_COND			41	/* R3 */

/* OCR bits */
#define MMC_OCR_MEM_READY		(1<<31)	/* memory power-up status bit */
#define MMC_OCR_3_5V_3_6V		(1<<23)
#define MMC_OCR_3_4V_3_5V		(1<<22)
#define MMC_OCR_3_3V_3_4V		(1<<21)
#define MMC_OCR_3_2V_3_3V		(1<<20)
#define MMC_OCR_3_1V_3_2V		(1<<19)
#define MMC_OCR_3_0V_3_1V		(1<<18)
#define MMC_OCR_2_9V_3_0V		(1<<17)
#define MMC_OCR_2_8V_2_9V		(1<<16)
#define MMC_OCR_2_7V_2_8V		(1<<15)
#define MMC_OCR_2_6V_2_7V		(1<<14)
#define MMC_OCR_2_5V_2_6V		(1<<13)
#define MMC_OCR_2_4V_2_5V		(1<<12)
#define MMC_OCR_2_3V_2_4V		(1<<11)
#define MMC_OCR_2_2V_2_3V		(1<<10)
#define MMC_OCR_2_1V_2_2V		(1<<9)
#define MMC_OCR_2_0V_2_1V		(1<<8)
#define MMC_OCR_1_9V_2_0V		(1<<7)
#define MMC_OCR_1_8V_1_9V		(1<<6)
#define MMC_OCR_1_7V_1_8V		(1<<5)
#define MMC_OCR_1_6V_1_7V		(1<<4)

#define SD_OCR_SDHC_CAP			(1<<30)
#define SD_OCR_VOL_MASK			0xFF8000 /* bits 23:15 */

/* R1 response type bits */
#define MMC_R1_READY_FOR_DATA		(1<<8)	/* ready for next transfer */
#define MMC_R1_APP_CMD			(1<<5)	/* app. commands supported */

/* 48-bit response decoding (32 bits w/o CRC) */
#define MMC_R1(resp)			((resp)[0])
#define MMC_R3(resp)			((resp)[0])
#define SD_R6(resp)			((resp)[0])

/* RCA argument and response */
#define MMC_ARG_RCA(rca)		((rca) << 16)
#define SD_R6_RCA(resp)			(SD_R6((resp)) >> 16)

/* bus width argument */
#define SD_ARG_BUS_WIDTH_1		0
#define SD_ARG_BUS_WIDTH_4		2

/* MMC R2 response (CSD) */
#define MMC_CSD_CSDVER(resp)		MMC_RSP_BITS((resp), 126, 2)
#define  MMC_CSD_CSDVER_1_0		1
#define  MMC_CSD_CSDVER_2_0		2
#define MMC_CSD_MMCVER(resp)		MMC_RSP_BITS((resp), 122, 4)
#define  MMC_CSD_MMCVER_1_0		0 /* MMC 1.0 - 1.2 */
#define  MMC_CSD_MMCVER_1_4		1 /* MMC 1.4 */
#define  MMC_CSD_MMCVER_2_0		2 /* MMC 2.0 - 2.2 */
#define  MMC_CSD_MMCVER_3_1		3 /* MMC 3.1 - 3.3 */
#define  MMC_CSD_MMCVER_4_0		4 /* MMC 4 */
#define MMC_CSD_READ_BL_LEN(resp)	MMC_RSP_BITS((resp), 80, 4)
#define MMC_CSD_C_SIZE(resp)		MMC_RSP_BITS((resp), 62, 12)
#define MMC_CSD_CAPACITY(resp)		((MMC_CSD_C_SIZE((resp))+1) << \
					 (MMC_CSD_C_SIZE_MULT((resp))+2))
#define MMC_CSD_C_SIZE_MULT(resp)	MMC_RSP_BITS((resp), 47, 3)

/* MMC v1 R2 response (CID) */
#define MMC_CID_MID_V1(resp)		MMC_RSP_BITS((resp), 104, 24)
#define MMC_CID_PNM_V1_CPY(resp, pnm)					\
	do {								\
		(pnm)[0] = MMC_RSP_BITS((resp), 96, 8);			\
		(pnm)[1] = MMC_RSP_BITS((resp), 88, 8);			\
		(pnm)[2] = MMC_RSP_BITS((resp), 80, 8);			\
		(pnm)[3] = MMC_RSP_BITS((resp), 72, 8);			\
		(pnm)[4] = MMC_RSP_BITS((resp), 64, 8);			\
		(pnm)[5] = MMC_RSP_BITS((resp), 56, 8);			\
		(pnm)[6] = MMC_RSP_BITS((resp), 48, 8);			\
		(pnm)[7] = '\0';					\
	} while (0)
#define MMC_CID_REV_V1(resp)		MMC_RSP_BITS((resp), 40, 8)
#define MMC_CID_PSN_V1(resp)		MMC_RSP_BITS((resp), 16, 24)
#define MMC_CID_MDT_V1(resp)		MMC_RSP_BITS((resp), 8, 8)

/* MMC v2 R2 response (CID) */
#define MMC_CID_MID_V2(resp)		MMC_RSP_BITS((resp), 120, 8)
#define MMC_CID_OID_V2(resp)		MMC_RSP_BITS((resp), 104, 16)
#define MMC_CID_PNM_V2_CPY(resp, pnm)					\
	do {								\
		(pnm)[0] = MMC_RSP_BITS((resp), 96, 8);			\
		(pnm)[1] = MMC_RSP_BITS((resp), 88, 8);			\
		(pnm)[2] = MMC_RSP_BITS((resp), 80, 8);			\
		(pnm)[3] = MMC_RSP_BITS((resp), 72, 8);			\
		(pnm)[4] = MMC_RSP_BITS((resp), 64, 8);			\
		(pnm)[5] = MMC_RSP_BITS((resp), 56, 8);			\
		(pnm)[6] = '\0';					\
	} while (0)
#define MMC_CID_PSN_V2(resp)		MMC_RSP_BITS((resp), 16, 32)

/* SD R2 response (CSD) */
#define SD_CSD_CSDVER(resp)		MMC_RSP_BITS((resp), 126, 2)
#define  SD_CSD_CSDVER_1_0		0
#define  SD_CSD_CSDVER_2_0		1
#define SD_CSD_TAAC(resp)		MMC_RSP_BITS((resp), 112, 8)
#define  SD_CSD_TAAC_1_5_MSEC		0x26
#define SD_CSD_NSAC(resp)		MMC_RSP_BITS((resp), 104, 8)
#define SD_CSD_SPEED(resp)		MMC_RSP_BITS((resp), 96, 8)
#define  SD_CSD_SPEED_25_MHZ		0x32
#define  SD_CSD_SPEED_50_MHZ		0x5a
#define SD_CSD_CCC(resp)		MMC_RSP_BITS((resp), 84, 12)
#define  SD_CSD_CCC_ALL			0x5f5
#define SD_CSD_READ_BL_LEN(resp)	MMC_RSP_BITS((resp), 80, 4)
#define SD_CSD_READ_BL_PARTIAL(resp)	MMC_RSP_BITS((resp), 79, 1)
#define SD_CSD_WRITE_BLK_MISALIGN(resp)	MMC_RSP_BITS((resp), 78, 1)
#define SD_CSD_READ_BLK_MISALIGN(resp)	MMC_RSP_BITS((resp), 77, 1)
#define SD_CSD_DSR_IMP(resp)		MMC_RSP_BITS((resp), 76, 1)
#define SD_CSD_C_SIZE(resp)		MMC_RSP_BITS((resp), 62, 12)
#define SD_CSD_CAPACITY(resp)		((SD_CSD_C_SIZE((resp))+1) << \
					 (SD_CSD_C_SIZE_MULT((resp))+2))
#define SD_CSD_V2_C_SIZE(resp)		MMC_RSP_BITS((resp), 48, 22)
#define SD_CSD_V2_CAPACITY(resp)	((SD_CSD_V2_C_SIZE((resp))+1) << 10) 
#define SD_CSD_V2_BL_LEN		0x9	/* 512 */
#define SD_CSD_VDD_R_CURR_MIN(resp)	MMC_RSP_BITS((resp), 59, 3)
#define SD_CSD_VDD_R_CURR_MAX(resp)	MMC_RSP_BITS((resp), 56, 3)
#define SD_CSD_VDD_W_CURR_MIN(resp)	MMC_RSP_BITS((resp), 53, 3)
#define SD_CSD_VDD_W_CURR_MAX(resp)	MMC_RSP_BITS((resp), 50, 3)
#define  SD_CSD_VDD_RW_CURR_100mA	0x7
#define  SD_CSD_VDD_RW_CURR_80mA	0x6
#define SD_CSD_C_SIZE_MULT(resp)	MMC_RSP_BITS((resp), 47, 3)
#define SD_CSD_ERASE_BLK_EN(resp)	MMC_RSP_BITS((resp), 46, 1)
#define SD_CSD_SECTOR_SIZE(resp)	MMC_RSP_BITS((resp), 39, 7) /* +1 */
#define SD_CSD_WP_GRP_SIZE(resp)	MMC_RSP_BITS((resp), 32, 7) /* +1 */
#define SD_CSD_WP_GRP_ENABLE(resp)	MMC_RSP_BITS((resp), 31, 1)
#define SD_CSD_R2W_FACTOR(resp)		MMC_RSP_BITS((resp), 26, 3)
#define SD_CSD_WRITE_BL_LEN(resp)	MMC_RSP_BITS((resp), 22, 4)
#define  SD_CSD_RW_BL_LEN_2G		0xa
#define  SD_CSD_RW_BL_LEN_1G		0x9
#define SD_CSD_WRITE_BL_PARTIAL(resp)	MMC_RSP_BITS((resp), 21, 1)
#define SD_CSD_FILE_FORMAT_GRP(resp)	MMC_RSP_BITS((resp), 15, 1)
#define SD_CSD_COPY(resp)		MMC_RSP_BITS((resp), 14, 1)
#define SD_CSD_PERM_WRITE_PROTECT(resp)	MMC_RSP_BITS((resp), 13, 1)
#define SD_CSD_TMP_WRITE_PROTECT(resp)	MMC_RSP_BITS((resp), 12, 1)
#define SD_CSD_FILE_FORMAT(resp)	MMC_RSP_BITS((resp), 10, 2)

/* SD R2 response (CID) */
#define SD_CID_MID(resp)		MMC_RSP_BITS((resp), 120, 8)
#define SD_CID_OID(resp)		MMC_RSP_BITS((resp), 104, 16)
#define SD_CID_PNM_CPY(resp, pnm)					\
	do {								\
		(pnm)[0] = MMC_RSP_BITS((resp), 96, 8);			\
		(pnm)[1] = MMC_RSP_BITS((resp), 88, 8);			\
		(pnm)[2] = MMC_RSP_BITS((resp), 80, 8);			\
		(pnm)[3] = MMC_RSP_BITS((resp), 72, 8);			\
		(pnm)[4] = MMC_RSP_BITS((resp), 64, 8);			\
		(pnm)[5] = '\0';					\
	} while (0)
#define SD_CID_REV(resp)		MMC_RSP_BITS((resp), 56, 8)
#define SD_CID_PSN(resp)		MMC_RSP_BITS((resp), 24, 32)
#define SD_CID_MDT(resp)		MMC_RSP_BITS((resp), 8, 12)

/* Might be slow, but it should work on big and little endian systems. */
#define MMC_RSP_BITS(resp, start, len)	__bitfield((resp), (start)-8, (len))
static __inline int
__bitfield(u_int32_t *src, int start, int len)
{
	u_int8_t *sp;
	u_int32_t dst, mask;
	int shift, bs, bc;

	if (start < 0 || len < 0 || len > 32)
		return 0;

	dst = 0;
	mask = len % 32 ? UINT_MAX >> (32 - (len % 32)) : UINT_MAX;
	shift = 0;

	while (len > 0) {
		sp = (u_int8_t *)src + start / 8;
		bs = start % 8;
		bc = 8 - bs;
		if (bc > len)
			bc = len;
		dst |= (*sp++ >> bs) << shift;
		shift += bc;
		start += bc;
		len -= bc;
	}

	dst &= mask;
	return (int)dst;
}

#endif
