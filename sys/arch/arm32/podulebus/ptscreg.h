/* $NetBSD: ptscreg.h,v 1.1 1996/01/31 23:26:35 mark Exp $ */

/*
 * Copyright (c) 1995 Scott Stevens
 * Copyright (c) 1995 Daniel Widenfalk
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
 *      This product includes software developed by Daniel Widenfalk
 *      for the NetBSD Project.
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
 * Power-tec SCSI-2 with FAS216 SCSI interface hardware description.
 */

#ifndef _PTSCREG_H_
#define _PTSCREG_H_

#include <arm32/podulebus/sfasvar.h>

typedef volatile unsigned short vu_short;

typedef struct ptsc_regmap {
	sfas_regmap_t	FAS216;
	vu_char		*chipreset;
	vu_char		*inten;
	vu_char		*status;
	vu_char		*term;
	vu_char		*led;
} ptsc_regmap_t;
typedef ptsc_regmap_t *ptsc_regmap_p;

/* NDA Information */
#define PTSC_CONTROL_CHIPRESET		0x1018
#define	PTSC_CONTROL_INTEN		0x101c
#define PTSC_STATUS			0x2000
#define PTSC_CONTROL_TERM		0x2018
#define PTSC_CONTROL_LED		0x201c
#define PTSC_FASOFFSET_BASE		0x3000
#define PTSC_FASOFFSET_TCL		0x0000
#define PTSC_FASOFFSET_TCM		0x0040
#define PTSC_FASOFFSET_FIFO		0x0080
#define PTSC_FASOFFSET_COMMAND		0x00c0
#define PTSC_FASOFFSET_DESTID		0x0100
#define PTSC_FASOFFSET_TIMEOUT		0x0140
#define PTSC_FASOFFSET_PERIOD		0x0180
#define PTSC_FASOFFSET_OFFSET		0x01c0
#define PTSC_FASOFFSET_CONFIG1		0x0200
#define PTSC_FASOFFSET_CLOCKCONV	0x0240
#define PTSC_FASOFFSET_TEST		0x0280
#define PTSC_FASOFFSET_CONFIG2		0x02c0
#define PTSC_FASOFFSET_CONFIG3		0x0300
#define PTSC_FASOFFSET_TCH		0x0380
#define PTSC_FASOFFSET_FIFOBOTTOM	0x03c0
/* NDA Info end */
#endif
