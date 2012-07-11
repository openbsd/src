/*	$OpenBSD: descr.c,v 1.5 2012/07/11 13:43:54 yuo Exp $	*/
/*	$NetBSD: descr.c,v 1.2 2002/02/20 20:31:07 christos Exp $	*/

/*
 * Copyright (c) 1999 Lennart Augustsson <augustss@netbsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include <dev/usb/usb.h>

#include "usbhid.h"
#include "usbvar.h"

int
hid_get_report_id(int fd)
{
	report_desc_t rep;
	hid_data_t d;
	hid_item_t h;
	int kindset;
	int temp = -1;
	int ret = -1;

	if ((rep = hid_get_report_desc(fd)) == NULL)
		goto use_ioctl;
	kindset = 1 << hid_input | 1 << hid_output | 1 << hid_feature;
	for (d = hid_start_parse(rep, kindset, 0); hid_get_item(d, &h); ) {
		/* Return the first report ID we met. */
		if (h.report_ID != 0) {
			temp = h.report_ID;
			break;
		}
	}
	hid_end_parse(d);
	hid_dispose_report_desc(rep);

	if (temp >0)
		return (temp);

use_ioctl:
	if(ioctl(fd, USB_GET_REPORT_ID, &temp) < 0)
		return 0;
	else
		ret = temp;


	return (ret);
}

report_desc_t
hid_get_report_desc(int fd)
{
	struct usb_ctl_report_desc rep;

	memset(&rep, 0, sizeof(rep));

	if (ioctl(fd, USB_GET_REPORT_DESC, &rep) < 0)
		return (NULL);

	/* check END_COLLECTION */
	if (((unsigned char *)rep.ucrd_data)[rep.ucrd_size -1] != 0xc0) {
		return (NULL);
	}

	return hid_use_report_desc(rep.ucrd_data, (unsigned int)rep.ucrd_size);
}

report_desc_t
hid_use_report_desc(unsigned char *data, unsigned int size)
{
	report_desc_t r;

	r = malloc(sizeof(*r) + size);
	if (r == NULL)
		return (NULL);
	r->size = size;
	memcpy(r->data, data, size);
	return (r);
}

void
hid_dispose_report_desc(report_desc_t r)
{

	free(r);
}
