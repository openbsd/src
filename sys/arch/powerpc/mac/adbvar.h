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

#define ADB_MAXTRACE	(NBPG / sizeof(int) - 1)
extern int adb_traceq[ADB_MAXTRACE];
extern int adb_traceq_tail;
extern int adb_traceq_len;

typedef struct adb_trace_xlate_s {
	int     params;
	char   *string;
}       adb_trace_xlate_t;

extern adb_trace_xlate_t adb_trace_xlations[];

#ifdef DEBUG
#ifndef ADB_DEBUG
#define ADB_DEBUG
#endif
#endif

#ifdef ADB_DEBUG
extern int	adb_debug;
#endif

typedef caddr_t Ptr;
typedef caddr_t *Handle;

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


/* adb.c */
void	adb_enqevent __P((adb_event_t *event));
void	adb_handoff __P((adb_event_t *event));
void	adb_autorepeat __P((void *keyp));
void	adb_dokeyupdown __P((adb_event_t *event));
void	adb_keymaybemouse __P((adb_event_t *event));
void	adb_processevent __P((adb_event_t *event));
int	adbopen __P((dev_t dev, int flag, int mode, struct proc *p));
int	adbclose __P((dev_t dev, int flag, int mode, struct proc *p));
int	adbread __P((dev_t dev, struct uio *uio, int flag));
int	adbwrite __P((dev_t dev, struct uio *uio, int flag));
int	adbioctl __P((dev_t , int , caddr_t , int , struct proc *));
int	adbpoll __P((dev_t dev, int events, struct proc *p));

/* adbsys.c */
void	adb_complete __P((caddr_t buffer, caddr_t data_area, int adb_command));
void	adb_msa3_complete __P((caddr_t buffer, caddr_t data_area, int adb_command));
void	adb_mm_nonemp_complete __P((caddr_t buffer, caddr_t data_area, int adb_command));
void	extdms_init __P((int));
void	extdms_complete __P((caddr_t, caddr_t, int));

/* types of adb hardware that we (will eventually) support */
#define ADB_HW_UNKNOWN		0x01	/* don't know */
#define ADB_HW_II		0x02	/* Mac II series */
#define ADB_HW_IISI		0x03	/* Mac IIsi series */
#define ADB_HW_PB		0x04	/* PowerBook series */
#define ADB_HW_CUDA		0x05	/* Machines with a Cuda chip */

#define ADB_CMDADDR(cmd)	((u_int8_t)((cmd) & 0xf0) >> 4)
#define ADBFLUSH(dev)		((((u_int8_t)(dev) & 0x0f) << 4) | 0x01)
#define ADBLISTEN(dev, reg)	((((u_int8_t)(dev) & 0x0f) << 4) | 0x08 | (reg))
#define ADBTALK(dev, reg)	((((u_int8_t)(dev) & 0x0f) << 4) | 0x0c | (reg))

/* adb_direct.c */
int	adb_poweroff __P((void));
int	CountADBs __P((void));
void	ADBReInit __P((void));
int	GetIndADB __P((ADBDataBlock * info, int index));
int	GetADBInfo __P((ADBDataBlock * info, int adbAddr));
int	SetADBInfo __P((ADBSetInfoBlock * info, int adbAddr));
int	ADBOp __P((Ptr buffer, Ptr compRout, Ptr data, short commandNum));
int	adb_read_date_time __P((unsigned long *t));
int	adb_set_date_time __P((unsigned long t));
