/*	$OpenBSD: atapi_cd.h,v 1.1 1999/07/20 06:21:59 csapuntz Exp $	*/
/*	$NetBSD: atapi_cd.h,v 1.9 1998/07/13 16:50:56 thorpej Exp $	*/

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

#define	ATAPI_LOAD_UNLOAD	0xa6
struct atapi_load_unload {
	u_int8_t opcode;
	u_int8_t unused1[3];
	u_int8_t options;
	u_int8_t unused2[3];
	u_int8_t slot;
	u_int8_t unused3[3];
};

struct atapi_cdrom_page {
	u_int8_t page;
	u_int8_t length;
	u_int8_t reserved;
	u_int8_t inact_mult;
	u_int8_t spm[2];
	u_int8_t fps[2];
};

struct atapi_cap_page {
	/* Capabilities page */
	u_int8_t page_code;
	u_int8_t param_len;  
	u_int8_t reserved1[2];

	u_int8_t cap1;
#define AUDIO_PLAY 0x01		/* audio play supported */
#define AV_COMPOSITE		/* composite audio/video supported */
#define DA_PORT1		/* digital audio on port 1 */
#define DA_PORT2		/* digital audio on port 2 */
#define M2F1			/* mode 2 form 1 (XA) read */
#define M2F2			/* mode 2 form 2 format */
#define CD_MULTISESSION		/* multi-session photo-CD */
	u_int8_t cap2;
#define CD_DA		0x01	/* audio-CD read supported */
#define CD_DA_STREAM	0x02	/* CD-DA streaming */
#define RW_SUB		0x04	/* combined R-W subchannels */
#define RW_SUB_CORR	0x08	/* R-W subchannel data corrected */
#define C2_ERRP		0x10	/* C2 error pointers supported */
#define ISRC		0x20	/* can return the ISRC */
#define UPC		0x40	/* can return the catalog number UPC */
	u_int8_t m_status;
#define CANLOCK		0x01	/* could be locked */
#define LOCK_STATE	0x02	/* current lock state */
#define PREVENT_JUMP	0x04	/* prevent jumper installed */
#define CANEJECT	0x08	/* can eject */
#define MECH_MASK	0xe0	/* loading mechanism type */
#define MECH_CADDY		0x00
#define MECH_TRAY		0x20
#define MECH_POPUP		0x40
#define MECH_CHANGER_INDIV	0x80
#define MECH_CHANGER_CARTRIDGE	0xa0
	u_int8_t cap3;
#define SEPARATE_VOL	0x01	/* independent volume of channels */
#define SEPARATE_MUTE	0x02	/* independent mute of channels */
#define SUPP_DISK_PRESENT 0x04	/* changer can report contents of slots */
#define SSS		0x08	/* software slot selection */
	u_int8_t max_speed[2];	/* max raw data rate in bytes/1000 */
	u_int8_t max_vol_levels[2]; /* number of discrete volume levels */
	u_int8_t buf_size[2];	/* internal buffer size in bytes/1024 */
	u_int8_t cur_speed[2];	/* current data rate in bytes/1000  */
	/* Digital drive output format description (optional?) */
	u_int8_t reserved2;
	u_int8_t dig_output; /* Digital drive output format description */
	u_int8_t reserved3[2];
};

#define ATAPI_CDROM_PAGE	0x0d
#define ATAPI_AUDIO_PAGE	0x0e
#define ATAPI_AUDIO_PAGE_MASK	0x4e
#define ATAPI_CAP_PAGE		0x2a

union atapi_cd_pages {
	u_int8_t page_code;
	struct atapi_cdrom_page cdrom;
	struct atapi_cap_page cap;
	struct cd_audio_page audio;
};

struct atapi_cd_mode_data {
	struct atapi_mode_header header;
	union atapi_cd_pages pages;
};

#define AUDIOPAGESIZE \
	(sizeof(struct atapi_mode_header) + sizeof(struct cd_audio_page))
#define CDROMPAGESIZE \
	(sizeof(struct atapi_mode_header) + sizeof(struct atapi_cdrom_page))
#define CAPPAGESIZE \
	(sizeof(struct atapi_mode_header) + sizeof(struct atapi_cap_page))
