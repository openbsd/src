/*	$OpenBSD: tea5757.h,v 1.1 2001/10/04 19:46:46 gluk Exp $	*/
/* $RuOBSD: tea5757.h,v 1.5 2001/10/04 18:51:50 pva Exp $ */

/*
 * Copyright (c) 2001 Vladimir Popov <jumbo@narod.ru>
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _TEA5757_H_
#define _TEA5757_H_

#include <sys/types.h>

#include <machine/bus.h>

#define TEA5757_REGISTER_LENGTH		25
#define TEA5757_FREQ			0x0007FFF
#define TEA5757_DATA			0x1FF8000

#define TEA5757_SEARCH_START		(1 << 24) /* 0x1000000 */
#define TEA5757_SEARCH_END		(0 << 24) /* 0x0000000 */

#define TEA5757_SEARCH_UP		(1 << 23) /* 0x0800000 */
#define TEA5757_SEARCH_DOWN		(0 << 23) /* 0x0000000 */
#define TEA5757_ACQUISITION_DELAY	100000
#define TEA5757_WAIT_DELAY		1000
#define TEA5757_SEARCH_DELAY		14	  /* 14 microseconds */

#define TEA5757_STEREO			(0 << 22) /* 0x0000000 */
#define TEA5757_MONO			(1 << 22) /* 0x0400000 */

#define TEA5757_BAND_FM			(0 << 20)
#define TEA5757_BAND_MW			(1 << 20)
#define TEA5757_BAND_LW			(2 << 20)
#define TEA5757_BAND_SW			(3 << 20)

#define TEA5757_USER_PORT		(0 << 18)
#define TEA5757_DUMMY			(0 << 15)

#define TEA5757_S005			(0 << 16) /* 0x0000000 * > 5 mkV */
#define TEA5757_S010			(2 << 16) /* 0x0020000 * > 10 mkV */
#define TEA5757_S030			(1 << 16) /* 0x0010000 * > 30 mkV */
#define TEA5757_S150			(3 << 16) /* 0x0030000 * > 150 mkV */

struct tea5757_t {
	bus_space_tag_t	iot;
	bus_space_handle_t	ioh;
	bus_size_t	offset;

	void	(*init)(bus_space_tag_t, bus_space_handle_t, bus_size_t,
			u_long); /* init value */
	void	(*rset)(bus_space_tag_t, bus_space_handle_t, bus_size_t,
			u_long); /* reset value */
	void	(*write_bit)(bus_space_tag_t, bus_space_handle_t, bus_size_t,
			u_char); /* the bit */
	u_long	(*read)(bus_space_tag_t, bus_space_handle_t, bus_size_t);
};

u_long	tea5757_encode_freq(u_long);
u_long	tea5757_decode_freq(u_long);
u_long	tea5757_encode_lock(u_char);
u_char	tea5757_decode_lock(u_long);

u_long	tea5757_set_freq(struct tea5757_t *, u_long, u_long, u_long);
void	tea5757_search(struct tea5757_t *, u_long, u_long, int);

void	tea5757_hardware_write(struct tea5757_t *, u_long);

#endif /* _TEA5757_H_ */
