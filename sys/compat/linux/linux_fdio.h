/*	$OpenBSD: linux_fdio.h,v 1.1 2001/04/09 06:53:44 tholo Exp $	*/
/*	$NetBSD: linux_fdio.h,v 1.1 2000/12/10 14:12:16 fvdl Exp $	*/

/*
 * Copyright (c) 2000 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUX_FDIO_H
#define _LINUX_FDIO_H

/*
 * Linux floppy ioctl call structures and defines.
 */

struct linux_floppy_struct {
	u_int size;
	u_int sect;
	u_int head;
	u_int track;
	u_int stretch;
	u_char gap;
	u_char rate;
	u_char spec1;
	u_char fmt_gap;
	const char *name;
};

struct linux_floppy_max_errors {
	u_int abort;
	u_int read_track;
	u_int reset;
	u_int recal;
	u_int reporting;
};

struct linux_floppy_drive_params {
	char cmos;
	u_long max_dtr;
	u_long hlt;
	u_long hut;
        u_long srt;
	u_long spinup;
	u_long spindown;
        u_char spindown_offset;
	u_char select_delay;
	u_char rps;
	u_char tracks;
	u_long timeout;
	u_char interleave_sect;
	struct linux_floppy_max_errors max_errors;
	char flags;
	char read_track;
	short autodetect[8];
	int checkfreq;
	int native_format;
};

struct linux_floppy_drive_struct {
	u_long flags;
	u_long spinup_date;
	u_long select_date;
	u_long first_read_date;
	short probed_format;
	short track;
	short maxblock;
	short maxtrack;
	int generation;
	int keep_data;
	int fd_ref;
	int fd_device;
	u_long last_checked;
	char *dmabuf;
	int bufblocks;
};

#define LINUX_FD_NEED_TWADDLE	0x01
#define LINUX_FD_VERIFY		0x02
#define LINUX_FD_DISK_NEWCHANGE	0x04
#define LINUX_FD_DISK_CHANGED	0x10
#define LINUX_FD_DISK_WRITABLE	0x20


struct linux_floppy_fdc_state {       
	int spec1;
	int spec2;
	int dtr;
	u_char version;
	u_char dor;
	u_long address;
	u_int rawcmd:2;
	u_int reset:1;
	u_int need_configure:1;
	u_int perp_mode:2;
	u_int has_fifo:1;
	u_int driver_version;
	u_char track[4];
};

struct linux_floppy_write_errors {
	u_int write_errors;
	u_long first_error_sector;
	u_int first_error_generation;
	u_long last_error_sector;
	u_int last_error_generation;
	u_int badness;
};

struct linux_floppy_raw_cmd {
	u_int flags;
	void *data;
	caddr_t kernel_data;
	struct floppy_raw_cmd *next;
	long length;
	long phys_length;
	int buffer_length;
	u_char rate;
	u_char cmd_count;
	u_char cmd[16];
	u_char reply_count;
	u_char reply[16];
	int track;
	int resultcode;
	int reserved1;
	int reserved2;
};

struct linux_format_descr {
	u_int device;
	u_int head;
	u_int track;
};

typedef char linux_floppy_drive_name[16];

#define LINUX_FDCLRPRM		_LINUX_IO(2, 0x41)
#define LINUX_FDSETPRM		_LINUX_IOW(2, 0x42, struct linux_floppy_struct)
#define LINUX_FDDEFPRM		_LINUX_IOW(2, 0x43, struct linux_floppy_struct)
#define LINUX_FDGETPRM		_LINUX_IOR(2, 0x04, struct linux_floppy_struct)
#define LINUX_FDMSGON		_LINUX_IO(2, 0x45)
#define LINUX_FDMSGOFF		_LINUX_IO(2, 0x46)
#define LINUX_FDFMTBEG		_LINUX_IO(2, 0x47)
#define LINUX_FDFMTTRK		_LINUX_IOW(2, 0x48, struct linux_format_descr)
#define LINUX_FDFMTEND		_LINUX_IO(2, 0x49)
#define LINUX_FDSETEMSGTRESH	_LINUX_IO(2, 0x4a)
#define LINUX_FDFLUSH		_LINUX_IO(2, 0x4b)
#define LINUX_FDSETMAXERRS \
	_LINUX_IOW(2, 0x4c, struct linux_floppy_max_errors)
#define LINUX_FDGETMAXERRS \
	_LINUX_IOR(2, 0x0e, struct linux_floppy_max_errors)
#define LINUX_FDGETDRVTYP	_LINUX_IOR(2, 0x0f, linux_floppy_drive_name)
/* 0x90 is not a typo, that's how it's listed in the Linux include file */
#define LINUX_FDSETDRVPRM \
	_LINUX_IOW(2, 0x90, struct linux_floppy_drive_params)
#define LINUX_FDGETDRVPRM \
	_LINUX_IOR(2, 0x11, struct linux_floppy_drive_params)
#define LINUX_FDGETDRVSTAT \
	_LINUX_IOR(2, 0x12, struct linux_floppy_drive_struct)
#define LINUX_FDPOLLDRVSTAT \
	_LINUX_IOR(2, 0x13, struct linux_floppy_drive_struct)
#define LINUX_FDRESET		_LINUX_IO(2, 0x54)
#define LINUX_FDGETFDCSTAT \
	_LINUX_IOR(2, 0x15, struct linux_floppy_fdc_state)
#define LINUX_FDWERRORCLR	_LINUX_IO(2, 0x56)
#define LINUX_FDWERRORGET \
	_LINUX_IOR(2, 0x17, struct linux_floppy_write_errors)
#define LINUX_FDRAWCMD		_LINUX_IO(2, 0x58)
#define LINUX_FDTWADDLE		_LINUX_IO(2, 0x59)
#define LINUX_FDEJECT		_LINUX_IO(2, 0x5a)

#endif /* _LINUX_FDIO_H */
