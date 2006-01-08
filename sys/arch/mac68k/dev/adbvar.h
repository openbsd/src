/*	$OpenBSD: adbvar.h,v 1.10 2006/01/08 17:45:29 miod Exp $	*/
/*	$NetBSD: adbvar.h,v 1.22 2005/01/15 16:00:59 chs Exp $	*/

/*
 * Copyright (C) 1994	Bradley A. Grantham
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
 *	This product includes software developed by Bradley A. Grantham.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <mac68k/mac68k/macrom.h>
#include <machine/adbsys.h>

/*
 * Arguments used to attach a device to the Apple Desktop Bus
 */
struct adb_attach_args {
	int	origaddr;
	int	adbaddr;
	int	handler_id;
};

extern int	adb_polling;

#ifdef DEBUG
#ifndef ADB_DEBUG
#define ADB_DEBUG
#endif
#endif

#ifdef ADB_DEBUG
extern int	adb_debug;
#endif

/* adb.c */
int	adb_op_sync(Ptr, Ptr, Ptr, short);
#ifdef MRG_ADB
void	adb_op_comprout(void);
#else
void	adb_op_comprout(caddr_t, caddr_t, int);
#endif

/* adbsysasm.s */
void	adb_kbd_asmcomplete(void);
void	adb_ms_asmcomplete(void);

/* types of adb hardware that we (will eventually) support */
#define ADB_HW_UNKNOWN		0x0	/* don't know */
#define ADB_HW_II		0x1	/* Mac II series */
#define ADB_HW_IISI		0x2	/* Mac IIsi series */
#define ADB_HW_PB		0x3	/* PowerBook series */
#define ADB_HW_CUDA		0x4	/* Machines with a Cuda chip */
#define	MAX_ADB_HW		4	/* Number of ADB hardware types */

#define	ADB_CMDADDR(cmd)	((u_int8_t)(cmd & 0xf0) >> 4)
#define	ADBFLUSH(dev)		((((u_int8_t)dev & 0x0f) << 4) | 0x01)
#define	ADBLISTEN(dev, reg)	((((u_int8_t)dev & 0x0f) << 4) | 0x08 | reg)
#define	ADBTALK(dev, reg)	((((u_int8_t)dev & 0x0f) << 4) | 0x0c | reg)

#ifndef MRG_ADB
/* adb_direct.c */
int	adb_poweroff(void);
int	CountADBs(void);
void	ADBReInit(void);
int	GetIndADB(ADBDataBlock *, int);
int	GetADBInfo(ADBDataBlock *, int);
int	SetADBInfo(ADBSetInfoBlock *, int);
int	ADBOp(Ptr, Ptr, Ptr, short);
int	adb_read_date_time(unsigned long *);
int	adb_set_date_time(unsigned long);
#endif /* !MRG_ADB */
