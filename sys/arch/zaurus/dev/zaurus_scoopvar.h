/*	$OpenBSD: zaurus_scoopvar.h,v 1.10 2005/11/17 05:26:31 uwe Exp $	*/

/*
 * Copyright (c) 2005 Uwe Stuehler <uwe@bsdx.de>
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

#define	SCOOP_LED_GREEN		(1<<0)
#define	SCOOP_LED_ORANGE	(1<<1)

void	scoop_set_backlight(int, int);
void	scoop_set_irled(int);
void	scoop_led_set(int, int);
void	scoop_battery_temp_adc(int);
void	scoop_charge_battery(int, int);
void	scoop_discharge_battery(int);
void	scoop_check_mcr(void);
void	scoop_set_headphone(int);
void	scoop_akin_pullup(int);
void	scoop_suspend(void);
void	scoop_resume(void);
