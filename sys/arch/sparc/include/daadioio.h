/*	$OpenBSD: daadioio.h,v 1.3 2003/06/02 18:40:59 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ioctls and flags for DAADIO.
 */

struct daadio_pio {
	u_int8_t	dap_reg;
	u_int8_t	dap_val;
};

struct daadio_adc {
	u_int8_t	dad_reg;
	u_int16_t	dad_val;
};

struct daadio_dac {
	u_int8_t	dac_reg;
	u_int16_t	dac_val;
};

#define	DIOGPIO		_IOWR('D', 0x01, struct daadio_pio) /* get pio val */
#define	DIOSPIO		 _IOW('D', 0x01, struct daadio_pio) /* set pio val */
#define	DIOGOPIO	_IOWR('D', 0x02, struct daadio_pio) /* get outp sts */
#define	DIOSOPIO	 _IOW('D', 0x02, struct daadio_pio) /* set to outp */
#define	DIOGADC		_IOWR('D', 0x03, struct daadio_adc) /* get adc val */
#define	DIOSDAC		 _IOW('D', 0x04, struct daadio_dac) /* set dac val */
