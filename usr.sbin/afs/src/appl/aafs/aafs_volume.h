/*
 * Copyright (c) 2002, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AAFS_VOLUME_H
#define AAFS_VOLUME_H 1

#include <aafs/aafs_cell.h>

struct aafs_volume;

/* 
 * create a `volume' object for `volumename' in `cell' with `flags',
 * volume information is fetched from the vldb and volser when needed
 */

int
aafs_volume_create(struct aafs_cell *cell,
		   const char *volumename, 
		   unsigned long flags,
		   struct aafs_volume **volume);

#define AAFS_VOL_CREATE_CREATE		1
#define AAFS_VOL_CREATE_NO_CANONICALIZE	2

int
aafs_volume_attach(struct aafs_cell *cell,
		   unsigned long flags,
		   nvldbentry *vldb,
		   struct aafs_volume **volume);


void
aafs_volume_free(struct aafs_volume *volume);

char *
aafs_volume_name(struct aafs_volume *volume, char *volname, size_t sz);

int
aafs_volume_examine_nvldb(struct aafs_volume *volume,
			  unsigned long flags,
			  struct nvldbentry *vldb);

int
aafs_volume_print_nvldb(struct aafs_volume *volume, FILE *f,
			unsigned long flags);

#define VOL_PRINT_VLDB_SKIP_NAME	1

struct aafs_volume_info;
struct aafs_volume_info_entry;
struct aafs_volume_info_ctx;

int
aafs_volume_examine_info(struct aafs_volume *volume,
			 unsigned long flags,
			 struct aafs_volume_info **info);

#define VOL_EXA_VOLINFO_RW	1
#define VOL_EXA_VOLINFO_RO	2
#define VOL_EXA_VOLINFO_BU	4

#define VOL_EXA_VOLINFO_ALL (VOL_EXA_VOLINFO_RW|VOL_EXA_VOLINFO_RO|VOL_EXA_VOLINFO_BU)

struct aafs_volume_info_entry *
aafs_volume_info_first(struct aafs_volume_info *,
		       struct aafs_volume_info_ctx **);

struct aafs_volume_info_entry *
aafs_volume_info_next(struct aafs_volume_info_ctx *ctx);

int
aafs_volume_info_destroy_ctx(struct aafs_volume_info_ctx *);

struct aafs_site *
aafs_volume_info_get_site(struct aafs_volume_info_entry *);

int
aafs_volume_info_get_volinfo(struct aafs_volume_info_entry *, xvolintInfo *);

#define AAFS_VOLUME_INFO_ENTRY_OK	0
#define AAFS_VOLUME_INFO_ENTRY_DEAD	1

int
aafs_volume_status_print(FILE *f, int flags, 
			 struct aafs_volume_info_entry *status);

#define AAFS_VOLUME_STATUS_PRINT_EXTENDED	1
#define AAFS_VOLUME_STATUS_PRINT_SUMMERY	2

int
aafs_volume_status_have_type(struct aafs_volume *volume, 
			     int volume_type);
int
aafs_volume_sitelist(struct aafs_volume *,
		     struct aafs_site_list **);


/*
 * modification ops
 */

int
aafs_volume_add_readonly(struct aafs_volume *,
			 struct aafs_site *);

int
aafs_volume_remove_readonly(struct aafs_volume *,
			    struct aafs_site *);

int
aafs_volume_move_readwrite(struct aafs_volume *volume,
			   struct aafs_site *site,
			   unsigned long flags);

#define AAFS_VOL_MOVE_MOVE_RO_TOO	0x01
#define AAFS_VOL_MOVE_NO_RELEASE	0x02

int
aafs_volume_release(struct aafs_volume *volume,
		    unsigned long flags);

#define AFS_VOL_RELEASE_FORCE		0x1

#if 0

/* I'm not sure this interface should openly be provided to
 * consumers */

int
aafs_volume_zap(struct aafs_volume *volume,
		int voltype,
		struct aafs_site *site);
int
aafs_volume_vldbremove(struct aafs_volume *volume,
		       int voltype,
		       struct aafs_site *site);
int
aafs_volume_lock(struct aafs_volume *volume);
int
aafs_volume_unlock(struct aafs_volume *volume);

#endif



/*
 */

#endif /* AAFS_VOLUME_H */
