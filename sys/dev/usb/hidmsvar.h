/*	$OpenBSD: hidmsvar.h,v 1.1 2010/07/31 16:04:50 miod Exp $ */
/*	$NetBSD: ums.c,v 1.60 2003/03/11 16:44:00 augustss Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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

#define MAX_BUTTONS	31	/* must not exceed size of sc_buttons */

struct hidms {
	int		sc_enabled;
	int		sc_flags;	/* device configuration */
#define HIDMS_SPUR_BUT_UP	0x01	/* spurious button up events */
#define HIDMS_Z			0x02	/* Z direction available */
#define HIDMS_REVZ		0x04	/* Z-axis is reversed */
#define HIDMS_W			0x08	/* W direction available */
#define HIDMS_REVW		0x10	/* W-axis is reversed */
#define HIDMS_LEADINGBYTE	0x20	/* Unknown leading byte */

	int		sc_num_buttons;
	u_int32_t	sc_buttons;	/* mouse button status */

	struct device	*sc_device;
	struct device	*sc_wsmousedev;

	/* locators */
	struct hid_location sc_loc_x;
	struct hid_location sc_loc_y;
	struct hid_location sc_loc_z;
	struct hid_location sc_loc_w;
	struct hid_location sc_loc_btn[MAX_BUTTONS];
};

void	hidms_attach(struct hidms *, const struct wsmouse_accessops *);
int	hidms_detach(struct hidms *, int);
void	hidms_disable(struct hidms *);
int	hidms_enable(struct hidms *);
void	hidms_input(struct hidms *, uint8_t *, u_int);
int	hidms_ioctl(struct hidms *, u_long, caddr_t, int, struct proc *);
int	hidms_setup(struct device *, struct hidms *, uint32_t, int, void *,
	    int);
