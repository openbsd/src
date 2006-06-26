/* $OpenBSD: prebind.h,v 1.2 2006/06/26 23:26:12 drahn Exp $ */
/*
 * Copyright (c) 2006 Dale Rahn <drahn@dalerahn.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define PREBIND_VERSION		2
struct prebind_footer {
	off_t	prebind_base;
	u_int32_t nameidx_idx;
	u_int32_t symcache_idx;
	u_int32_t pltsymcache_idx;
	u_int32_t fixup_idx;
	u_int32_t nametab_idx;
	u_int32_t fixupcnt_idx;
	u_int32_t libmap_idx;

	u_int32_t symcache_cnt;
	u_int32_t pltsymcache_cnt;
	u_int32_t fixup_cnt;
	u_int32_t numlibs;
	u_int32_t prebind_size;

	u_int32_t id0;
	u_int32_t id1;
	/* do not modify or add fields below this point in the struct */
	off_t	orig_size;
	u_int32_t prebind_version;
	char bind_id[4];
#define BIND_ID0 'P'
#define BIND_ID1 'R'
#define BIND_ID2 'E'
#define BIND_ID3 'B'
};


struct nameidx {
	u_int32_t name;
	u_int32_t id0;
	u_int32_t id1;
};

struct symcachetab {
	u_int32_t idx;
	u_int32_t obj_idx;
	u_int32_t sym_idx;
};

struct fixup {
	u_int32_t sym;
	u_int32_t obj_idx;
	u_int32_t sym_idx;
};

int prebind_delete(char **argv);
int prebind(char **argv);
int	prebind_remove_load_section(int fd, char *name);
