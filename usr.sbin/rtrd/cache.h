/*	$OpenBSD: cache.h,v 1.1 2026/09/16 16:11:46 job Exp $ */
/*
 * Copyright (c) 2025-2026 Ralph Covelli <rcovelli@he.net>
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

extern void init_cache_frame(struct cache_frame *);
extern int init_cache(struct cache *, uint16_t, uint16_t);
extern int init_cache_array(uint16_t);
extern int is_cache_empty(struct cache *);
extern void free_cache_frame(struct cache_frame *);
extern struct cache_frame *get_latest_cache_frame(uint8_t);
extern struct cache_frame *get_serial_cache_frame(uint8_t, uint32_t);
extern struct cache_frame *cache_frame_push(uint8_t,
    struct cache_vrp4_tree *, struct cache_vrp6_tree *,
    struct cache_brk_tree *, struct cache_vap_tree *);
extern void update_cache(struct vrp4_tree *, struct vrp6_tree *,
    struct brk_tree *, struct vap_tree *);

extern struct cache cache[];
