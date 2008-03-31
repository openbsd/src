/*	$OpenBSD: ds1687reg.h,v 1.2 2008/03/31 07:14:00 jsing Exp $ */

/*
 * Copyright (c) 2007, Joel Sing <jsing@openbsd.org>
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
 * Register locations/definitions for the Dallas Semiconductor (now Maxim)
 * DS1685 RTC chip and DS1687 stand-alone RTC EDIP. Product details are
 * available at:
 *
 *    http://www.maxim-ic.com/quick_view2.cfm/qv_pk/2757
 *
 * Full data sheet is available from:
 *
 *    http://datasheets.maxim-ic.com/en/ds/DS1685-DS1687.pdf
 *
 * The DS1687 contains a DS1685, which is an improved version of the older
 * DS1287 RTC. New/extended registers are available by selecting bank 1.
 * Register values are either in BCD (data mode 0) or binary (data mode 1).
 */

/*
 * DS1687 Registers.
 */
#define DS1687_SEC		0x00	/* Seconds. */
#define DS1687_SEC_ALRM		0x01	/* Alarm seconds. */
#define DS1687_MIN		0x02	/* Minutes. */
#define DS1687_MIN_ALRM		0x03	/* Alarm minutes. */
#define DS1687_HOUR		0x04	/* Hours. */
#define DS1687_HOUR_ALRM	0x05	/* Alarm hours. */
#define DS1687_DOW		0x06	/* Day of week (01-07). */
#define DS1687_DAY		0x07	/* Day (01-31). */
#define DS1687_MONTH		0x08	/* Month (01-12). */
#define DS1687_YEAR		0x09	/* Year (00-99). */

#define DS1687_CTRL_A		0x0a	/* Control register A. */
#define   DS1687_BANK_1		0x10	/* Bank select. */
#define   DS1687_UIP		0x80	/* Update in progress. */
#define DS1687_CTRL_B		0x0b	/* Control register B. */
#define   DS1687_24_HR		0x02	/* Use 24 hour time. */
#define   DS1687_DM_1		0x04	/* Data mode 1 (binary). */
#define   DS1687_SET_CLOCK	0x80	/* Prohibit updates. */
#define DS1687_CTRL_C		0x0c	/* Control register C. */
#define DS1687_CTRL_D		0x0d	/* Control register D. */

#define DS1687_CENTURY		0x48	/* Century (bank 1). */
#define DS1687_DATE_ALRM	0x49	/* Date alarm (bank 1). */

#define DS1687_EXT_CTRL		0x4a	/* Extended control register. */
#define   DS1687_KICKSTART	0x01	/* Kickstart flag. */

