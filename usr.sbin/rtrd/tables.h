/*	$OpenBSD: tables.h,v 1.1 2026/09/16 16:11:46 job Exp $ */
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

/* VRP4 */

extern RB_PROTOTYPE(vrp4_tree, vrp4, entry, vrp4cmp)

extern int insert_vrp4(struct vrp4_tree *, struct vrp4 *, time_t);
extern int remove_vrp4(struct vrp4_tree *, struct vrp4 *);
extern void free_vrp4_tree(struct vrp4_tree *);
extern int vrp4_treecmp(struct vrp4_tree *, struct vrp4_tree *);
extern void vrp4_treecpy(struct vrp4_tree *, struct vrp4_tree *);
extern void vrp4_treecpy_expire(struct vrp4_tree *, struct vrp4_tree *, time_t);
extern int64_t vrp4_treecount(struct vrp4_tree *);
extern int vrp4_treeempty(struct vrp4_tree *);
extern void vrp4_treeupdate(struct vrp4_tree *, struct vrp4_tree *);
extern uint32_t vrp4_treehash(struct vrp4_tree *);
extern void vrp4_treecpy_count_and_hash(
    struct vrp4_tree *,
    struct vrp4_tree *,
    int64_t *,
    uint32_t *);

extern struct cache_vrp4_tree *cache_vrp4_tree_find(struct vrp4_tree *);
extern struct cache_vrp4_tree *cache_vrp4_tree_dup(struct cache_vrp4_tree *);
extern struct cache_vrp4_tree *cache_vrp4_tree_new(struct vrp4_tree *);
extern void cache_vrp4_tree_free(struct cache_vrp4_tree *);

/* VRP6 */

extern RB_PROTOTYPE(vrp6_tree, vrp6, entry, vrp6cmp)

extern int insert_vrp6(struct vrp6_tree *, struct vrp6 *, time_t);
extern int remove_vrp6(struct vrp6_tree *, struct vrp6 *);
extern void free_vrp6_tree(struct vrp6_tree *);
extern int vrp6_treecmp(struct vrp6_tree *, struct vrp6_tree *);
extern void vrp6_treecpy(struct vrp6_tree *, struct vrp6_tree *);
extern void vrp6_treecpy_expire(struct vrp6_tree *, struct vrp6_tree *, time_t);
extern int64_t vrp6_treecount(struct vrp6_tree *);
extern int vrp6_treeempty(struct vrp6_tree *);
extern void vrp6_treeupdate(struct vrp6_tree *, struct vrp6_tree *);
extern uint32_t vrp6_treehash(struct vrp6_tree *);
extern void vrp6_treecpy_count_and_hash(
    struct vrp6_tree *,
    struct vrp6_tree *,
    int64_t *,
    uint32_t *);

extern struct cache_vrp6_tree *cache_vrp6_tree_find(struct vrp6_tree *);
extern struct cache_vrp6_tree *cache_vrp6_tree_dup(struct cache_vrp6_tree *);
extern struct cache_vrp6_tree *cache_vrp6_tree_new(struct vrp6_tree *);
extern void cache_vrp6_tree_free(struct cache_vrp6_tree *);

/* BRK */

extern RB_PROTOTYPE(brk_tree, brk, entry, brkcmp)

extern int insert_brk(struct brk_tree *, struct brk *, time_t);
extern int remove_brk(struct brk_tree *, struct brk *);
extern void free_brk_tree(struct brk_tree *);
extern int brk_treecmp(struct brk_tree *, struct brk_tree *);
extern void brk_treecpy(struct brk_tree *, struct brk_tree *);
extern void brk_treecpy_expire(struct brk_tree *, struct brk_tree *, time_t);
extern int64_t brk_treecount(struct brk_tree *);
extern int brk_treeempty(struct brk_tree *);
extern void brk_treeupdate(struct brk_tree *, struct brk_tree *);
extern uint32_t brk_treehash(struct brk_tree *);
extern void brk_treecpy_count_and_hash(
    struct brk_tree *,
    struct brk_tree *,
    int64_t *,
    uint32_t *);

extern struct cache_brk_tree *cache_brk_tree_find(struct brk_tree *);
extern struct cache_brk_tree *cache_brk_tree_dup(struct cache_brk_tree *);
extern struct cache_brk_tree *cache_brk_tree_new(struct brk_tree *);
extern void cache_brk_tree_free(struct cache_brk_tree *);

/* ASN */

extern RB_PROTOTYPE(asn_tree, asn, entry, asncmp)

extern int insert_asn(struct asn_tree *, uint32_t);
extern int remove_asn(struct asn_tree *, uint32_t);
extern void free_asn_tree(struct asn_tree *);
extern int clean_asn_tree(struct asn_tree *);
extern int32_t asn_treecount(struct asn_tree *);

/* VAP */

extern RB_PROTOTYPE(vap_tree, vap, entry, vapcmp)

extern int vapfullcmp(struct vap *, struct vap *);
extern int insert_vap(struct vap_tree *, struct vap *, time_t);
extern int remove_vap(struct vap_tree *, struct vap *);
extern void free_vap_tree(struct vap_tree *);
extern int vap_treecmp(struct vap_tree *, struct vap_tree *);
extern void vap_treecpy(struct vap_tree *, struct vap_tree *);
extern void vap_treecpy_expire(struct vap_tree *, struct vap_tree *, time_t);
extern int64_t vap_treecount(struct vap_tree *);
extern int vap_treeempty(struct vap_tree *);
extern void vap_treeupdate(struct vap_tree *, struct vap_tree *);
extern uint32_t vap_treehash(struct vap_tree *);
extern void vap_treecpy_count_and_hash(
    struct vap_tree *,
    struct vap_tree *,
    int64_t *,
    uint32_t *);

extern struct cache_vap_tree *cache_vap_tree_find(struct vap_tree *);
extern struct cache_vap_tree *cache_vap_tree_dup(struct cache_vap_tree *);
extern struct cache_vap_tree *cache_vap_tree_new(struct vap_tree *);
extern void cache_vap_tree_free(struct cache_vap_tree *);
