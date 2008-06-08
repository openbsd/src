/* $OpenBSD: extern.h,v 1.10 2008/06/08 21:01:24 av Exp $ */
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
	u_int blklen;
	int   fd;
	char *file;
	SLIST_ENTRY(track_info) track_list;
	char type;
};
SLIST_HEAD(track_head, track_info) tracks;

/* Media capabilities */
#define MEDIACAP_TAO			0x01
#define MEDIACAP_CDRW_WRITE		0x02

extern unsigned long 	entry2time(struct cd_toc_entry *);
extern unsigned long 	entry2frames(struct cd_toc_entry *);
extern int              open_cd(char *, int);
extern char ** 		cddb(const char *, int, struct cd_toc_entry *, char *);
extern unsigned long 	cddb_discid(int, struct cd_toc_entry *);
extern void		free_names(char **);
extern int		get_media_capabilities(int *cap);
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
