/*	$OpenBSD: ieee1212var.h,v 1.2 2002/12/13 02:52:11 tdeval Exp $	*/
/*	$NetBSD: ieee1212var.h,v 1.1 2002/02/27 04:58:51 jmc Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by James Chacon.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_DEV_STD_IEEE1212VAR_H
#define	_DEV_STD_IEEE1212VAR_H

struct p1212_dir;

typedef struct p1212_key {
	u_int8_t key_type;
	u_int8_t key_value;
	u_int8_t key;
	u_int32_t val;
} p1212_key;

typedef struct p1212_leafdata {
	u_int32_t len;
	u_int32_t *data;
} p1212_leafdata;

typedef struct p1212_textdata {
	u_int8_t spec_type;
	u_int32_t spec_id;
	u_int32_t lang_id;
	char *text;
} p1212_textdata;

typedef struct p1212_com {
	struct p1212_key key;
	u_int32_t textcnt;
	struct p1212_textdata **text;
} p1212_com;

typedef struct p1212_data {
	struct p1212_com com;

	u_int32_t val;
	struct p1212_leafdata *leafdata;
	void (*print)(struct p1212_data *);
	TAILQ_ENTRY(p1212_data) data;
} p1212_data;

typedef struct p1212_dir {
	struct p1212_com com;

	int match;
	void (*print)(struct p1212_dir *);
	struct p1212_dir *parent;
	TAILQ_HEAD(, p1212_data) data_root;
	TAILQ_HEAD(, p1212_dir) subdir_root;
	TAILQ_ENTRY(p1212_dir) dir;
} p1212_dir;

typedef struct p1212_rom {
	char name[5];
	u_int32_t len;
	u_int32_t *data;
	struct p1212_dir *root;
} p1212_rom;

int p1212_iscomplete(u_int32_t *, u_int32_t *);
struct p1212_rom *p1212_parse(u_int32_t *, u_int32_t, u_int32_t);
void p1212_walk(struct p1212_dir *, void *,
    void (*)(struct p1212_key *, void *));
struct p1212_key **p1212_find(struct p1212_dir *, int, int, int);
void p1212_print(struct p1212_dir *);
void p1212_free(struct p1212_rom *);
struct device **p1212_match_units(struct device *, struct p1212_dir *,
    int (*)(void *, const char *));

#endif	/* _DEV_STD_IEEE1212VAR_H */
