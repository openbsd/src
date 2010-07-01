/*	$OpenBSD: mouse.c,v 1.10 2010/07/01 16:47:58 maja Exp $	*/
/*	$NetBSD: mouse.c,v 1.3 1999/11/15 13:47:30 ad Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Juergen Hannken-Illjes.
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

#include <sys/ioctl.h>
#include <sys/time.h>
#include <dev/wscons/wsconsio.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include "wsconsctl.h"

static int mstype;
static int resolution;
static int samplerate;
static int rawmode;

struct wsmouse_calibcoords wmcoords, wmcoords_save; 

struct field mouse_field_tab[] = {
    { "resolution",		&resolution,	FMT_UINT,	FLG_WRONLY },
    { "samplerate",		&samplerate,	FMT_UINT,	FLG_WRONLY },
    { "type",			&mstype,	FMT_MSTYPE,	FLG_RDONLY },
    { "rawmode",		&rawmode,	FMT_UINT,	FLG_MODIFY|FLG_INIT},
    { "scale",			&wmcoords,	FMT_SCALE,	FLG_MODIFY|FLG_INIT},
    { NULL }
};

void
mouse_get_values(const char *pre, int fd)
{
	if (field_by_value(mouse_field_tab, &mstype)->flags & FLG_GET)
		if (ioctl(fd, WSMOUSEIO_GTYPE, &mstype) < 0)
			warn("WSMOUSEIO_GTYPE");
	
	if (field_by_value(mouse_field_tab, &rawmode)->flags & FLG_GET) { 
		if (ioctl(fd, WSMOUSEIO_GCALIBCOORDS, &wmcoords) < 0) {
			if (errno == ENOTTY)
				field_by_value(mouse_field_tab,
				    &rawmode)->flags |= FLG_DEAD;
			else
				warn("WSMOUSEIO_GCALIBCOORDS");
		}
		rawmode = wmcoords.samplelen;
	}

	if (field_by_value(mouse_field_tab, &wmcoords)->flags & FLG_GET)
		if (ioctl(fd, WSMOUSEIO_GCALIBCOORDS, &wmcoords) < 0) {
			if (errno == ENOTTY)
				field_by_value(mouse_field_tab,
				    &wmcoords)->flags |= FLG_DEAD;
			else
				warn("WSMOUSEIO_GCALIBCOORDS");
	}	
}

int
mouse_put_values(const char *pre, int fd)
{
	if (field_by_value(mouse_field_tab, &resolution)->flags & FLG_SET) {
		if (ioctl(fd, WSMOUSEIO_SRES, &resolution) < 0) {
			warn("WSMOUSEIO_SRES");
			return 1;
		}
	}
	if (field_by_value(mouse_field_tab, &samplerate)->flags & FLG_SET) {
		if (ioctl(fd, WSMOUSEIO_SRATE, &samplerate) < 0) {
			warn("WSMOUSEIO_SRATE");
			return 1;
		}
	}
	if (field_by_value(mouse_field_tab, &rawmode)->flags & FLG_SET) { 
		wmcoords.samplelen = rawmode;
		if (ioctl(fd, WSMOUSEIO_SCALIBCOORDS, &wmcoords) < 0) {
			if (errno == ENOTTY) {
				field_by_value(mouse_field_tab,
				    &rawmode)->flags |= FLG_DEAD;
			} else {
				warn("WSMOUSEIO_SCALIBCOORDS");
				return 1;
			}
		}
	}
	if (field_by_value(mouse_field_tab, &wmcoords)->flags & FLG_SET) {
		if (ioctl(fd, WSMOUSEIO_GCALIBCOORDS, &wmcoords_save) < 0) 
			if (errno == ENOTTY)
				field_by_value(mouse_field_tab,
				    &wmcoords)->flags |= FLG_DEAD;
			else
				warn("WSMOUSEIO_GCALIBCOORDS");
		wmcoords.samplelen = wmcoords_save.samplelen;
		if (ioctl(fd, WSMOUSEIO_SCALIBCOORDS, &wmcoords) < 0) {
			if (errno == ENOTTY) {
				field_by_value(mouse_field_tab,
				    &wmcoords)->flags |= FLG_DEAD;
			} else {
				warn("WSMOUSEIO_SCALIBCOORDS");
				return 1;
			}
		}
	}

	return 0;
}

int
mouse_next_device(int *index)
{
	char devname[20];
	int fd;

	snprintf(devname, sizeof(devname), "/dev/wsmouse%d", *index);

	if ((fd = open(devname, O_WRONLY)) < 0 &&
	    (fd = open(devname, O_RDONLY)) < 0) {
		if (errno != ENXIO)
			*index = -1;
	}
	return(fd);
}
