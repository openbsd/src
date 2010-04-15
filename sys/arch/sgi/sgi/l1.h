/*	$OpenBSD: l1.h,v 1.4 2010/04/15 20:32:50 miod Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
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
 * High-level L1 communication defines
 */

/* L1 command types and destination addresses */
#define	L1_ADDRESS(type,addr)	(((type) << 28) | (addr))

#define	L1_TYPE_L1	0x00
#define	L1_TYPE_L2	0x01
#define	L1_TYPE_L3	0x02
#define	L1_TYPE_CBRICK	0x03
#define	L1_TYPE_IOBRICK	0x04

#define	L1_ADDRESS_RACK_MASK	0x0ffc0000
#define	L1_ADDRESS_RACK_SHIFT		18
#define	L1_ADDRESS_RACK_LOCAL		0x3ff
#define	L1_ADDRESS_BAY_MASK	0x0003f000
#define	L1_ADDRESS_BAY_SHIFT		12
#define	L1_ADDRESS_BAY_LOCAL		0x3f
#define	L1_ADDRESS_TASK_MASK	0x0000001f
#define	L1_ADDRESS_TASK_SHIFT		0

#define	L1_ADDRESS_LOCAL \
	((L1_ADDRESS_RACK_LOCAL << L1_ADDRESS_RACK_SHIFT) | \
	 (L1_ADDRESS_BAY_LOCAL << L1_ADDRESS_BAY_SHIFT))

#define	L1_TASK_INVALID		0x00
#define	L1_TASK_ROUTER		0x01
#define	L1_TASK_SYSMGMT		0x02
#define	L1_TASK_COMMAND		0x03
#define	L1_TASK_ENVIRONMENT	0x04
#define	L1_TASK_BEDROCK		0x05
#define	L1_TASK_GENERAL		0x06

/* response codes */
#define	L1_RESP_OK		((uint32_t)0)
#define	L1_RESP_NXDATA		((uint32_t)-0x68)
#define	L1_RESP_INVAL		((uint32_t)-0x6b)

/*
 * Various commands (very incomplete list)
 */

/* L1_TASK_COMMAND requests */
#define	L1_REQ_EXEC_CMD	0x0000	/* interpret plaintext command */

/* L1_TASK_GENERAL requests */
#define	L1_REQ_EEPROM	0x0006	/* access eeprom */
#define	L1_REQ_DISP1	0x1004	/* display text on LCD first line */
#define	L1_REQ_DISP2	0x1005	/* display text on LCD second line */

/* L1_REQ_EEPROM additional argument value */
/* non C-brick component */
#define	L1_EEP_POWER	0x00		/* power board */
#define	L1_EEP_LOGIC	0x01		/* logic board */
/* C-brick component */
#define	L1_EEP_DIMM_NOINTERLEAVE_BASE	0x04
#define	L1_EEP_DIMM_INTERLEAVE_BASE	0x05
#define	L1_EEP_DIMM_NOINTERLEAVE(d) \
	(L1_EEP_DIMM_NOINTERLEAVE_BASE + (d))
#define	L1_EEP_DIMM_INTERLEAVE(d) \
	(L1_EEP_DIMM_INTERLEAVE_BASE + ((d) >> 1) + ((d) & 0x01 ? 4 : 0))
/* ia code */
#define	L1_EEP_CHASSIS	0x01		/* chassis ia */
#define	L1_EEP_BOARD	0x02		/* board ia */
#define	L1_EEP_IUSE	0x03		/* internal use ia */
#define	L1_EEP_SPD	0x04		/* spd record */

#define	L1_SPD_DIMM_MAX	8

struct spdmem_attach_args {
	struct mainbus_attach_args	maa;
	int				dimm;
};

int	l1_exec_command(int16_t, const char *);
int	l1_get_brick_ethernet_address(int16_t, uint8_t *);
int	l1_get_brick_spd_record(int16_t, int, u_char **, size_t *);
int	l1_read_board_ia(int16_t, u_char **, size_t *);
