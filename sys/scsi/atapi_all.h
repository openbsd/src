/*	$OpenBSD: atapi_all.h,v 1.1 1999/07/20 06:21:59 csapuntz Exp $	*/
/*	$NetBSD: atapi_all.h,v 1.3 1998/02/13 08:28:16 enami Exp $	*/

/*
 * Copyright (c) 1996 Manuel Bouyer.  All rights reserved.
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
 *	This product includes software developed by Manuel Bouyer.
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

struct scsi_link;

#define ATAPI_MODE_SELECT	0x55
struct atapi_mode_select {
	u_int8_t opcode;
	u_int8_t byte2;
#define AMS_SP  0x01			/* save pages */
#define AMS_PF  0x10			/* must be set in byte2 */
	u_int8_t reserved1[5];
	u_int8_t length[2];
	u_int8_t reserved2[3];
};

#define ATAPI_MODE_SENSE	0x5a
struct atapi_mode_sense {
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t page;
	u_int8_t reserved1[4];
	u_int8_t length[2];
	u_int8_t reserved2[3];
};

struct atapi_mode_header {
	u_int8_t length[2];
	u_int8_t medium;
#define MDT_UNKNOWN	0x00
#define MDT_DATA_120	0x01
#define MDT_AUDIO_120	0x02
#define MDT_COMB_120	0x03
#define MDT_PHOTO_120	0x04
#define MDT_DATA_80	0x05
#define MDT_AUDIO_80	0x06
#define MDT_COMB_80	0x07
#define MDT_PHOTO_80	0x08
#define MDT_NO_DISC	0x70
#define MDT_DOOR_OPEN	0x71
#define MDT_FMT_ERROR	0x72
	u_int8_t reserved[5];
};

int	atapi_mode_select __P((struct scsi_link *,
	    struct atapi_mode_header *, int, int, int, int));
int	atapi_mode_sense __P((struct scsi_link *, int,
	    struct atapi_mode_header *, int, int, int, int));
