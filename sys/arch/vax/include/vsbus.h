/*	$NetBSD: vsbus.h,v 1.1 1996/07/20 17:58:28 ragge Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by Bertram Barth.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Generic definitions for the (virtual) vsbus. contains common info
 * used by all VAXstations.
 */
struct confargs {
	char	ca_name[16];		/* device name */
	int	ca_intslot;		/* device interrupt-slot */
	int	ca_intpri;		/* device interrupt "priority" */
	int	ca_intvec;		/* interrup-vector offset */
	int	ca_intbit;		/* bit in interrupt-register */
	int	ca_ioaddr;		/* device hardware I/O address */

	int	ca_aux1;		/* additional info (DMA, etc.) */
	int	ca_aux2;
	int	ca_aux3;
	int	ca_aux4;
	int	ca_aux5;
	int	ca_aux6;
	int	ca_aux7;
	int	ca_aux8;

#define ca_recvslot	ca_intslot	/* DC/DZ: Receiver configuration */
#define ca_recvpri	ca_intpri
#define ca_recvvec	ca_intvec
#define ca_recvbit	ca_intbit
#define ca_xmitslot	ca_aux1		/* DC/DZ: transmitter configuration */
#define ca_xmitpri	ca_aux2		/* DC/DZ:  */
#define ca_xmitvec	ca_aux3
#define ca_xmitbit	ca_aux4
#define ca_dcflags	ca_aux5

#define ca_dareg	ca_aux1		/* SCSI: DMA address register */
#define ca_dcreg	ca_aux2		/* SCSI: DMA byte count register */
#define ca_ddreg	ca_aux3		/* SCSI: DMA transfer direction */
#define ca_dbase	ca_aux4		/* SCSI: DMA buffer address */
#define ca_dsize	ca_aux5		/* SCSI: DMA buffer size */
#define ca_dflag	ca_aux6		/* SCSI: DMA flags (eg. shared) */
#define ca_idval	ca_aux7		/* SCSI: host-ID to use/set */
#define ca_idreg	ca_aux8		/* SCSI: host-ID port register */

#define ca_enaddr	ca_aux1		/* LANCE: Ethernet address in ROM */
#define ca_leflags	ca_aux2
};

int vsbus_intr_register __P((struct confargs *, int(*)(void*), void*));
int vsbus_intr_enable __P((struct confargs *));
int vsbus_intr_disable  __P((struct confargs *));
int vsbus_intr_unregister __P((struct confargs *));

int vsbus_lockDMA __P((struct confargs *));
int vsbus_unlockDMA __P((struct confargs *));

