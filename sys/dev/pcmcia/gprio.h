/*	$OpenBSD: gprio.h,v 1.3 2002/09/25 19:09:02 fgsch Exp $	*/

/*
 * Copyright (c) 2002, Federico G. Schwindt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * A driver for the Gemplus GPR400 SmartCard reader.
 *
 * The gpr400 driver written by Wolf Geldmacher <wgeldmacher@paus.ch> for
 * Linux was used as documentation.
 */

#ifndef _DEV_PCMCIA_GPRIO_H
#define _DEV_PCMCIA_GPRIO_H

typedef struct gpr400_ram {
	u_int8_t		ram[2016];
} gpr400_ram_t;

/*
 * gpr device operations.
 */
#define GPR_CLOSE		_IO('g', 1)
#define GPR_CMD			_IO('g', 2)
#define GPR_OPEN		_IO('g', 3)
#define GPR_POWER		_IOW('g', 4, int)
#define GPR_RAM			_IOR('g', 5, struct gpr400_ram)
#define GPR_RESET		_IO('g', 6)
#define GPR_SELECT		_IO('g', 7)
#define GPR_STATUS		_IO('g', 8)
#define GPR_TLV			_IO('g', 9)

#endif /* _DEV_PCMCIA_GPRIO_H */
