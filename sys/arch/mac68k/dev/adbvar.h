/*	$OpenBSD: adbvar.h,v 1.8 2003/09/23 16:51:11 millert Exp $	*/
/*	$NetBSD: adbvar.h,v 1.5 1997/01/13 07:01:24 scottr Exp $	*/

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

#define ADB_MAXTRACE	(NBPG / sizeof(int) - 1)
extern int adb_traceq[ADB_MAXTRACE];
extern int adb_traceq_tail;
extern int adb_traceq_len;

typedef struct adb_trace_xlate_s {
	int     params;
	char   *string;
}       adb_trace_xlate_t;

extern adb_trace_xlate_t adb_trace_xlations[];

/* adb.c */
void    adb_asmcomplete(void);
void	adb_enqevent(adb_event_t *event);
void	adb_handoff(adb_event_t *event);
void	adb_autorepeat(void *keyp);
void	adb_dokeyupdown(adb_event_t *event);
void	adb_keymaybemouse(adb_event_t *event);
void	adb_processevent(adb_event_t *event);
int	adbopen(dev_t dev, int flag, int mode, struct proc *p);
int	adbclose(dev_t dev, int flag, int mode, struct proc *p);
int	adbread(dev_t dev, struct uio *uio, int flag);
int	adbwrite(dev_t dev, struct uio *uio, int flag);
int	adbioctl(dev_t , int , caddr_t , int , struct proc *);
int	adbpoll(dev_t dev, int rw, struct proc *p);

/* adbsysadm.s */
void	extdms_complete(void);

/* adbsys.c */
void	adb_complete(caddr_t buffer, caddr_t data_area, int adb_command);
void	extdms_init(int);

#ifndef MRG_ADB

/* types of adb hardware that we (will eventually) support */
#define ADB_HW_UNKNOWN		0x01	/* don't know */
#define ADB_HW_II		0x02	/* Mac II series */
#define ADB_HW_IISI		0x03	/* Mac IIsi series */
#define ADB_HW_PB		0x04	/* PowerBook series */
#define ADB_HW_CUDA		0x05	/* Machines with a Cuda chip */

/* adb_direct.c */
int	adb_poweroff(void);
int	CountADBs(void);
void	ADBReInit(void);
int	GetIndADB(ADBDataBlock * info, int index);
int	GetADBInfo(ADBDataBlock * info, int adbAddr);
int	SetADBInfo(ADBSetInfoBlock * info, int adbAddr);
int	ADBOp(Ptr buffer, Ptr compRout, Ptr data, short commandNum);
int	adb_read_date_time(unsigned long *t);
int	adb_set_date_time(unsigned long t);

#endif
