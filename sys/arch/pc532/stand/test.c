/*	$NetBSD: test.c,v 1.2 1994/10/26 08:25:59 cgd Exp $	*/

/*-
 * Copyright (c) 1994 Philip L. Budne.
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
 *	This product includes software developed by Philip L. Budne.
 * 4. The name of Philip L. Budne may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PHILIP BUDNE ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PHILIP BUDNE BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Allow test of "boot" (and presumably other "stand" utils)
 * under user mode.
 *
 * Phil Budne <phil@ultimate.com> May 10, 1994
 */

/* my /sys/sys/syscall.h is out of sync w.r.t. lseek?? */
#include "/usr/include/sys/syscall.h"
#include <sys/types.h>

extern int errno;

int testing = 1;
int bootdev;
int howto;

_open( char *fname, int mode )
{
	return syscall( SYS_open, fname, mode );
}

_read( int fd, char *buf, int len )
{
	return syscall( SYS_read, fd, buf, len );
}

_write( int fd, char *buf, int len )
{
	return syscall( SYS_write, fd, buf, len );
}

/*
 * I'd like to strangle the jerk who thought it
 * was a cool idea to change lseek, rather than creating
 * a new "qseek" to handle long long offsets
 */
_lseek( int fd, off_t pos, int whence )
{
	return syscall( SYS_lseek, fd, 0, pos, whence );
}

int fd = -1;

opendisk(int unit)
{
	char fname[32];
	static int _unit = -1;

	if (unit == _unit)
		return;

	if (fd >= 0) {
		close(fd);
		fd = -1;
	}
	_unit = unit;

#if 0
	sprintf( fname, "/dev/r%s%dc", "sd", unit );
#else
	strcpy( fname, "/dev/rsd0c" );
#endif
	fd = _open( fname, 0 );
	if (fd < 0) {
		printf("open %s failed\n", fname );
		return;
	}
	printf("opened %s (fd %d)\n", fname, fd );
}

void
bzero( char *addr, int len )
{
    while (len-- > 0)
	*addr++ = '\0';
}

/* XXX TEMP; would like to use code more like hp300 scsi.c */

void
scsiinit(void)
{
}

int
scsialive(int ctlr)
{
	return 1;		/* controller always alive! */
}

#define BPS 512
scsi_tt_read(ctlr, slave, buf, len, blk, nblk)
	char *buf;
{
	int pos;
#if 0
printf("scsi_tt_read(ctlr %d, slave %d, buf 0x%x, len %d, blk %d, nblk %d)\n",
	ctlr, slave, buf, len, blk, nblk);
#endif

	opendisk(slave);

	pos = _lseek( fd, blk * BPS, 0 );
	if (pos != blk * BPS) {
		printf("lseek pos %d error %d\n", pos, errno );
		return errno;
	}
	if (_read( fd, buf, nblk * BPS ) != nblk * BPS) {
		printf("read errno %d\n", errno );
		return errno;
	}
	return 0;
}

scsi_tt_write()
{
	return -1;
}

#include <sys/types.h>
#include <dev/cons.h>

scnprobe(cp)
	struct consdev *cp;
{
	/* the only game in town */
	cp->cn_pri = CN_NORMAL;		/* XXX remote? */
}

scninit(cp)
	struct consdev *cp;
{
}

scnputchar(c)
	int c;
{
	char c2;
	c2 = c;
	_write(0, &c2, 1);
}

scngetchar()
{
	char c;
	_read(0, &c, 1);
	return c;
}

_rtt() {
	syscall( SYS_exit, 1 );
}

alloc(int size)
{
	return malloc(size);
}
