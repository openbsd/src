/*	$OpenBSD: umidivar.h,v 1.6 2002/09/08 01:09:07 deraadt Exp $ */
/*	$NetBSD: umidivar.h,v 1.4 2002/07/11 21:14:32 augustss Exp $ */
/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Takuya SHIOZAKI (tshiozak@netbsd.org).
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
 *	  This product includes software developed by the NetBSD
 *	  Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* pending MUX-MIDI packet */
typedef enum {
	PS_EXCL_0=-2,	/* put, and next state is PS_EXCL_0 */
	PS_END=-1,	/* put, and next state is PS_INITIAL */
	PS_INITIAL=0,	/* 0>= : not put, and state is keeped */
	PS_NORMAL_1OF3=1,
	PS_NORMAL_2OF3=2,
	PS_NORMAL_1OF2=3,
	PS_EXCL_1=4,
	PS_EXCL_2=5
} packet_state_t;

#define UMIDI_PACKET_SIZE 4
struct umidi_packet {
	char		buffer[UMIDI_PACKET_SIZE];
	packet_state_t	state;
};

/*
 * hierarchie
 *
 * <-- parent	       child -->
 *
 * umidi(sc) -> endpoint -> jack   <- (dynamically assignable) - mididev
 *	   ^	 |    ^	    |
 *	   +-----+    +-----+
 */

/* midi device */
struct umidi_mididev {
	struct umidi_softc	*sc;
	struct device		*mdev;
	/* */
	struct umidi_jack	*in_jack;
	struct umidi_jack	*out_jack;
	/* */
	int			opened;
	int			flags;
};

/* Jack Information */
struct umidi_jack {
	struct umidi_endpoint	*endpoint;
	/* */
	int			cable_number;
	struct umidi_packet	packet;
	void			*arg;
	int			binded;
	int			opened;
	union {
		struct {
			void			(*intr)(void *);
			LIST_ENTRY(umidi_jack)	queue_entry;
		} out;
		struct {
			void			(*intr)(void *, int);
		} in;
	} u;
};

#define UMIDI_MAX_EPJACKS	16
/* endpoint data */
struct umidi_endpoint {
	struct umidi_softc	*sc;
	/* */
	int			addr;
	usbd_pipe_handle	pipe;
	usbd_xfer_handle	xfer;
	char			*buffer;
	int			num_open;
	int			num_jacks;
	struct umidi_jack	*jacks[UMIDI_MAX_EPJACKS];
	LIST_HEAD(, umidi_jack)	queue_head;
	struct umidi_jack	*queue_tail;
};

/* software context */
struct umidi_softc {
	USBBASEDEVICE		sc_dev;
	usbd_device_handle	sc_udev;
	usbd_interface_handle	sc_iface;
	struct umidi_quirk	*sc_quirk;

	int			sc_dying;

	int			sc_out_num_jacks;
	struct umidi_jack	*sc_out_jacks;
	int			sc_in_num_jacks;
	struct umidi_jack	*sc_in_jacks;
	struct umidi_jack	*sc_jacks;

	int			sc_num_mididevs;
	struct umidi_mididev	*sc_mididevs;

	int			sc_out_num_endpoints;
	struct umidi_endpoint	*sc_out_ep;
	int			sc_in_num_endpoints;
	struct umidi_endpoint	*sc_in_ep;
	struct umidi_endpoint	*sc_endpoints;
};
