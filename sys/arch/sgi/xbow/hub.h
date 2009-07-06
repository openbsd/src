/*	$OpenBSD: hub.h,v 1.4 2009/07/06 22:46:43 miod Exp $	*/

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
 *  HUB access macros.
 */
#define	LWIN_SIZE_BITS		24
#define	LWIN_SIZE		(1ULL << LWIN_SIZE_BITS)

#define	NODE_LWIN_BASE(nasid, widget)                               \
        (IP27_NODE_IO_BASE(nasid) + ((uint64_t)(widget) << LWIN_SIZE_BITS))

#define	IP27_LHUB_ADDR(_x) \
	((volatile uint64_t *)(NODE_LWIN_BASE(0, 1) + (_x)))
#define	IP27_RHUB_ADDR(_n, _x) \
	((volatile uint64_t *)(NODE_LWIN_BASE(_n, 1) + 0x800000 + (_x)))
#define	IP27_RHUB_PI_ADDR(_n, _sn, _x) \
	((volatile uint64_t *)(NODE_LWIN_BASE(_n, 1) + 0x800000 + \
	    ((_sn) ? HUBPI_OFFSET : 0) + (_x)))

#define	IP27_LHUB_L(r)			*(IP27_LHUB_ADDR(r))
#define	IP27_LHUB_S(r, d)		*(IP27_LHUB_ADDR(r)) = (d)
#define	IP27_RHUB_L(n, r)		*(IP27_RHUB_ADDR((n), (r)))
#define	IP27_RHUB_S(n, r, d)		*(IP27_RHUB_ADDR((n), (r))) = (d)
#define IP27_RHUB_PI_L(n, s, r)		*(IP27_RHUB_PI_ADDR((n), (s), (r)))
#define	IP27_RHUB_PI_S(n, s, r, d)	*(IP27_RHUB_PI_ADDR((n), (s), (r))) = (d)

/*
 * IP27 specific registers
 */

/*
 * HUB space (very incomplete)
 */

/*
 * HUB PI - Processor Interface
 */

#define	HUBPIBASE			0x00000000

#define	HUBPI_REGION_PRESENT		0x00000018
#define	HUBPI_CPU_NUMBER		0x00000020
#define	HUBPI_CALIAS_SIZE		0x00000028
#define PI_CALIAS_SIZE_0			0


#define	HUBPI_CPU0_PRESENT		0x00000040
#define	HUBPI_CPU1_PRESENT		0x00000048
#define	HUBPI_CPU0_ENABLED		0x00000050
#define	HUBPI_CPU1_ENABLED		0x00000058

#define	HUBPI_IR_CHANGE			0x00000090
#define	PI_IR_SET				0x100
#define	PI_IR_CLR				0x000
#define	HUBPI_IR0			0x00000098
#define	HUBPI_IR1			0x000000a0
#define	HUBPI_CPU0_IMR0			0x000000a8
#define	HUBPI_CPU0_IMR1			0x000000b0
#define	HUBPI_CPU1_IMR0			0x000000b8
#define	HUBPI_CPU1_IMR1			0x000000c0

/*
 * Offset to use to access the second PI over the remote hub interface
 * on IP35.
 */
#define	HUBPI_OFFSET			0x00200000

/*
 * HUB MD - Memory/Directory
 */

#define	HUBMDBASE_IP27			0x00200000
#define	HUBMDBASE_IP35			0x00780000


/*
 * HUB IO - Widget I/O
 */

#define	HUBIOBASE			0x00400000

#define	HUBIO_IOTTE(x)			(0x00000160 + (x) * 8)
#define	IOTTE_MAX				7
#define	IOTTE_SWIN0				(IOTTE_MAX - 1)

#define	IOTTE(space,widget,offset) \
	(((space) << 12) | ((widget) << 8) | (offset))
#define	IOTTE_SPACE_DEVICE		1
#define	IOTTE_SPACE_MEMORY		0
#define	IOTTE_SPACE(iotte)		(((iotte) >> 12) & 0x01)
#define	IOTTE_WIDGET(iotte)		(((iotte) >> 8) & 0x0f)
#define	IOTTE_OFFSET(iotte)		((iotte) & 0xff)


/*
 * HUB LB - Local Bedrock
 */

#define	HUBLBBASE_IP35			0x00600000

/*
 * HUB NI - Network Interface
 */

#define	HUBNIBASE_IP27			0x00600000
#define	HUBNIBASE_IP35			0x00680000

#define	HUBNI_STATUS			0x00000000
#define	NI_MORENODES				0x0000000000040000
#define	HUBNI_RESET			0x00000008
#define	NI_RESET_ACTION_IP27			0x02
#define	NI_RESET_PORT_IP27			0x80
#define	NI_RESET_LOCAL_IP27			0x01
#define	NI_RESET_ACTION_IP35			0x01
#define	NI_RESET_PORT_IP35			0x02
#define	NI_RESET_LOCAL_IP35			0x04
#define	HUBNI_RESET_ENABLE		0x00000010
#define	NI_RESET_ENABLE				0x01

/*
 * HUB XB - Crossbar interface
 */

#define	HUBXBBASE_IP35			0x00700000
