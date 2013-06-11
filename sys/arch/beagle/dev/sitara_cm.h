/* $NetBSD: sitara_cm.h,v 1.1 2013/04/17 14:31:02 bouyer Exp $ */
/*
 * Copyright (c) 2010
 *	Ben Gray <ben.r.gray@gmail.com>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ben Gray.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BEN GRAY ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BEN GRAY BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


/**
 *	Functions to configure the PIN multiplexing on the chip.
 *
 *	This is different from the GPIO module in that it is used to configure the
 *	pins between modules not just GPIO input output.
 *
 */
#ifndef _OMAP_SCM_H_
#define _OMAP_SCM_H_

struct sitara_cm_padconf {
	uint16_t    reg_off;
	uint16_t    gpio_pin;
	uint16_t    gpio_mode;
	const char  *ballname;
	const char  *muxmodes[8];
};

struct sitara_cm_padstate {
	const char  *state;
	uint16_t    reg;
};

struct sitara_cm_device {
	uint16_t			padconf_muxmode_mask;
	uint16_t			padconf_sate_mask;
	struct sitara_cm_padstate	*padstate;
	struct sitara_cm_padconf		*padconf;
};

int sitara_cm_padconf_set(const char *padname, const char *muxmode, 
    unsigned int state);
int sitara_cm_padconf_get(const char *padname, const char **muxmode,
    unsigned int *state);
int sitara_cm_padconf_set_gpiomode(uint32_t gpio, unsigned int state);
int sitara_cm_padconf_get_gpiomode(uint32_t gpio, unsigned int *state);
int sitara_cm_padconf_set_gpioflags(uint32_t gpio, uint32_t flags);
void sitara_cm_padconf_get_gpioflags(uint32_t gpio, uint32_t *flags);
int sitara_cm_reg_read_4(uint32_t reg, uint32_t *val);
int sitara_cm_reg_write_4(uint32_t reg, uint32_t val);

#endif /* _OMAP_SCM_H_ */
