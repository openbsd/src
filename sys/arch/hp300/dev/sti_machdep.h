/*	$OpenBSD: sti_machdep.h,v 1.1 2011/08/18 20:02:57 miod Exp $	*/

/*
 * Copyright (c) 2005, 2011, Miodrag Vallat
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
 *
 */

#ifndef STANDALONE
/* Console data */
extern	struct sti_rom sticn_rom;
extern	struct sti_screen sticn_scr;
extern	bus_addr_t sticn_bases[STI_REGION_MAX];

void	sticninit(void);
int	sti_dio_probe(int);
int	sti_sgc_probe(bus_space_tag_t, int);
#endif

/* DIO attachment defines */
#define	STI_DIO_SCODE_OFFSET	0x02	/* offset to SGC rom, in select codes */
#define	STI_DIO_SIZE		0x10	/* expected total device size
					   in DIO-II size units */
