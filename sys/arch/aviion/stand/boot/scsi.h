/*	$OpenBSD: scsi.h,v 1.4 2013/10/16 16:59:34 miod Exp $	*/

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

/*
 * The following are defines from <scsi/scsiconf.h> not available to
 * !_KERNEL code, under the following licence terms:
 */

/*	OpenBSD: scsiconf.h,v 1.157 2013/09/27 11:43:19 krw Exp	*/
/*	$NetBSD: scsiconf.h,v 1.35 1997/04/02 02:29:38 mycroft Exp $	*/

/*
 * Copyright (c) 1993, 1994, 1995 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Originally written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

#define	ITSDONE		0x00008	/* the transfer is as done as it gets	*/
#define	SCSI_DATA_IN	0x00800	/* expect data to come INTO memory	*/
#define	SCSI_DATA_OUT	0x01000	/* expect data to flow OUT of memory	*/

#define XS_NOERROR	0	/* there is no error, (sense is invalid)  */
#define XS_SENSE	1	/* Check the returned sense for the error */
#define	XS_DRIVER_STUFFUP 2	/* Driver failed to perform operation	  */
#define XS_SELTIMEOUT	3	/* The device timed out.. turned off?	  */
#define XS_TIMEOUT	4	/* The Timeout reported was caught by SW  */
#define XS_BUSY		5	/* The device busy, try again later?	  */
#define XS_SHORTSENSE   6	/* Check the ATAPI sense for the error */
#define XS_RESET	8	/* bus was reset; possible retry command  */
#define XS_NO_CCB	9	/* device should requeue io and retry */

#define TEST_READY_RETRIES	5

#define SCSI_RETRIES		4

#include <scsi/scsi_message.h>

#include <sys/disklabel.h>

struct scsi_private {
	void	*scsicookie;
	int	(*scsicmd)(void *, void *, size_t, void *, size_t, size_t *);
	void	(*scsidetach)(void *);

	struct disklabel label;
	int	part;
};

struct scsi_private *
	scsi_initialize(const char *, int, int, int, int);

int	scsi_tur(struct scsi_private *);
int	scsi_read(struct scsi_private *, daddr32_t, size_t, void *, size_t *);

#include "oaic.h"
#include "oosiop.h"
