/*	$OpenBSD: adbvar.h,v 1.6 2005/10/21 22:07:45 kettenis Exp $	*/
/*	$NetBSD: adbvar.h,v 1.3 2000/06/08 22:10:46 tsubai Exp $	*/

/*-
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

#include <machine/adbsys.h>

/*
 * Arguments used to attach a device to the Apple Desktop Bus
 */
struct adb_attach_args {
	int	origaddr;
	int	adbaddr;
	int	handler_id;
};

#ifdef DEBUG
#ifndef ADB_DEBUG
#define ADB_DEBUG
#endif
#endif

extern int	adb_polling;	/* Are we polling?  (Debugger mode) */
#ifdef ADB_DEBUG
extern int	adb_debug;
#endif

typedef caddr_t Ptr;

/* ADB Manager */
typedef struct {
	Ptr siServiceRtPtr;
	Ptr siDataAreaAddr;
} ADBSetInfoBlock;
typedef struct {
	unsigned char	devType;
	unsigned char	origADBAddr;
	Ptr		dbServiceRtPtr;
	Ptr		dbDataAreaAddr;
} ADBDataBlock;

struct adb_softc {
	struct device sc_dev;
	char *sc_regbase;
};

/* adb_direct.c */
extern int adbHardware;

/* types of adb hardware that we (will eventually) support */
#define ADB_HW_UNKNOWN		0x01	/* don't know */
#define ADB_HW_PMU		0x04	/* PowerBook series */
#define ADB_HW_CUDA		0x05	/* Machines with a Cuda chip */

#define ADB_CMDADDR(cmd)	((u_int8_t)((cmd) & 0xf0) >> 4)
#define ADBFLUSH(dev)		((((u_int8_t)(dev) & 0x0f) << 4) | 0x01)
#define ADBLISTEN(dev, reg)	((((u_int8_t)(dev) & 0x0f) << 4) | 0x08 | (reg))
#define ADBTALK(dev, reg)	((((u_int8_t)(dev) & 0x0f) << 4) | 0x0c | (reg))

int	adb_poweroff(void);
int	CountADBs(void);
void	ADBReInit(void);
int	GetIndADB(ADBDataBlock * info, int index);
int	GetADBInfo(ADBDataBlock * info, int adbAddr);
int	SetADBInfo(ADBSetInfoBlock * info, int adbAddr);
int	adb_read_date_time(time_t *t);
int	adb_set_date_time(time_t t);
int	adb_intr(void *arg);
void	adb_cuda_autopoll(void);
int	adb_op_sync(Ptr, Ptr, Ptr, short);
