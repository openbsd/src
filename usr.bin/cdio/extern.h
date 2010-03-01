/* $OpenBSD: extern.h,v 1.15 2010/03/01 02:09:44 krw Exp $ */
/*
 * Copyright (c) 2002 Marc Espie.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/queue.h>

struct cd_toc_entry;
struct track_info {
	off_t sz;
	off_t off;
	u_int blklen;
	int   fd;
	char *file;
	SLIST_ENTRY(track_info) track_list;
	char type;
	int   speed; 
};
SLIST_HEAD(track_head, track_info) tracks;

/* Read/Write speed */
#define DRIVE_SPEED_MAX		0xfffe
#define DRIVE_SPEED_OPTIMAL	0xffff	/* automatically adjusted by drive */

/* Convert writing speed into Kbytes/sec (1x - 75 frames per second) */
#define CD_SPEED_TO_KBPS(x, blksz)	((x) * 75 * (blksz) / 1024)

/*
 * It's maximum possible speed for CD (audio track).
 * Data tracks theoretically can be written at 436x but in practice I
 * believe, 380x will be never reached.
 * NOTE: this value must never be changed to a bigger value, it can cause
 * DRIVE_SPEED_MAX overrun.
 */
#define CD_MAX_SPEED		380

/* MMC feature codes */
#define MMC_FEATURE_CDRW_CAV	0x27	/* Constant Angular Velocity */
#define MMC_FEATURE_CD_TAO	0x2d	/* Track-At-Once writing mode */
#define MMC_FEATURE_CDRW_WRITE	0x37	/* media is CD-RW and can be written */

#define MMC_FEATURE_MAX		0x0110

/* Media types */
#define MEDIATYPE_UNKNOWN	0
#define MEDIATYPE_CDR		1
#define MEDIATYPE_CDRW		2

extern unsigned long 	entry2time(struct cd_toc_entry *);
extern unsigned long 	entry2frames(struct cd_toc_entry *);
extern int              open_cd(char *, int);
extern char ** 		cddb(const char *, int, struct cd_toc_entry *, char *);
extern unsigned long 	cddb_discid(int, struct cd_toc_entry *);
extern void		free_names(char **);
extern int		get_media_type(void);
extern int		get_media_capabilities(u_int8_t *, int);
extern int		blank(void);
extern int		unit_ready(void);
extern int		synchronize_cache(void);
extern int		close_session(void);
extern int		get_disc_size(off_t *);
extern int		get_nwa(int *);
extern int		writetao(struct track_head *);
extern int		writetrack(struct track_info *, int);
extern int		mode_sense_write(unsigned char []);
extern int		mode_select_write(unsigned char []);
extern int		cdrip(char *);
extern int		cdplay(char *);

#define VERSION "2.1"
#define WAVHDRLEN 44
