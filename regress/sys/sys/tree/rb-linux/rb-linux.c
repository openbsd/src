/*	$OpenBSD: rb-test.c,v 1.5 2023/12/29 02:37:39 aisha Exp $	*/
/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
#include <sys/time.h>

#include <assert.h>
#include <err.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

struct timespec start, end, diff, rstart, rend, rdiff, rtot = {0, 0};
#ifndef timespecsub
#define	timespecsub(tsp, usp, vsp)					\
	do {								\
		(vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;		\
		(vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;	\
		if ((vsp)->tv_nsec < 0) {				\
			(vsp)->tv_sec--;				\
			(vsp)->tv_nsec += 1000000000L;			\
		}							\
	} while (0)
#endif
#ifndef timespecadd
#define	timespecadd(tsp, usp, vsp)					\
	do {								\
		(vsp)->tv_sec = (tsp)->tv_sec + (usp)->tv_sec;		\
		(vsp)->tv_nsec = (tsp)->tv_nsec + (usp)->tv_nsec;	\
		if ((vsp)->tv_nsec >= 1000000000L) {			\
			(vsp)->tv_sec++;				\
			(vsp)->tv_nsec -= 1000000000L;			\
		}							\
	} while (0)
#endif

//#define RB_SMALL
//#define RB_TEST_RANK
#define RB_TEST_DIAGNOSTIC
#define _RB_DIAGNOSTIC

#ifdef DOAUGMENT
#define RB_AUGMENT(elm) tree_augment(elm)
#endif

#define bool int

#include "linux/container_of.h"
#include "linux/rbtree.h"

#define TDEBUGF(fmt, ...)						\
	fprintf(stderr, "%s:%d:%s(): " fmt "\n",			\
        __FILE__, __LINE__, __func__, ##__VA_ARGS__)


#ifdef __OpenBSD__
#define SEED_RANDOM srandom_deterministic
#else
#define SEED_RANDOM srandom
#endif

int ITER=150000;
int RANK_TEST_ITERATIONS=10000;

#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* declarations */
struct node;
static void mix_operations(int *, int, struct node *, int, int, int, int);

#ifdef DOAUGMENT
static int tree_augment(struct node *);
#else
#define tree_augment(x) (0)
#endif

#ifdef RB_TEST_DIAGNOSTIC
static void print_helper(const struct rb_node *, int);
static void print_tree(const struct rb_root_cached *);
#else
#define print_helper(x, y)      do {} while (0)
#define print_tree(x)	   do {} while (0)
#endif

/* definitions */
struct node {
	struct rb_node	 node_link;
	int		 key;
	size_t		 height;
	size_t		 size;
};

struct rb_root_cached root = RB_ROOT_CACHED;

#ifndef RB_RANK
#define RB_RANK(x, y)   0
#endif
#ifndef _RB_GET_RDIFF2
#define _RB_GET_RDIFF2(x, y) 0ul
#endif

int j;

int
tree_rank(const struct rb_root_cached *t)
{
	struct rb_node *tree_root = t->rb_root.rb_node;
	return RB_RANK(linux_root, tree_root);
}

int
panic_cmp(struct rb_node *one, struct rb_node *two)
{
	errx(1, "panic_cmp called");
}

#undef RB_ROOT
#define RB_ROOT(head)   (head)->rbh_root
RB_GENERATE(linux_root, rb_node, __entry, panic_cmp);

static void
insert(struct node *node, struct rb_root_cached *tree_root)
{
	struct rb_node **new = &tree_root->rb_root.rb_node, *parent = NULL;
	int key = node->key;

	while (*new) {
		parent = *new;
		if (key < rb_entry(parent, struct node, node_link)->key)
			new = &parent->rb_left;
		else
			new = &parent->rb_right;
	}

	rb_link_node(&node->node_link, parent, new);
	rb_insert_color(&node->node_link, &tree_root->rb_root);
}

static void
erase(struct node *node, struct rb_root_cached *tree_root)
{
	rb_erase(&node->node_link, &tree_root->rb_root);
}

int
main(int argc, char **argv)
{
	char *test_target = NULL;
	struct node *tmp, *ins, *nodes;
	int i, r, rank, *perm, *nums;

	if (argc > 1)
		test_target = argv[1];

	nodes = calloc((ITER + 5), sizeof(struct node));
	perm = calloc(ITER, sizeof(int));
	nums = calloc(ITER, sizeof(int));

	// for determinism
	SEED_RANDOM(4201);

	TDEBUGF("generating a 'random' permutation");
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	perm[0] = 0;
	nums[0] = 0;
	for(i = 1; i < ITER; i++) {
		r = random() % i; // arc4random_uniform(i);
		perm[i] = perm[r];
		perm[r] = i;
		nums[i] = i;
	}
	/*
	fprintf(stderr, "{");
	for(int i = 0; i < ITER; i++) {
		fprintf(stderr, "%d, ", perm[i]);
	}
	fprintf(stderr, "}\n");
	int nperm[10] = {2, 4, 9, 7, 8, 3, 0, 1, 6, 5};
	int nperm[6] = {2, 6, 1, 4, 5, 3};
	int nperm[10] = {10, 3, 7, 8, 6, 1, 9, 2, 5, 4};
	int nperm[2] = {0, 1};

	int nperm[100] = {
		54, 47, 31, 35, 40, 73, 29, 66, 15, 45, 9, 71, 51, 32, 28, 62,
		12, 46, 50, 26, 36, 91, 10, 76, 33, 43, 34, 58, 55, 72, 37, 24,
		75, 4, 90, 88, 30, 25, 82, 18, 67, 81, 80, 65, 23, 41, 61, 86,
		20, 99, 59, 14, 79, 21, 68, 27, 1, 7, 94, 44, 89, 64, 96, 2, 49,
		53, 74, 13, 48, 42, 60, 52, 95, 17, 11, 0, 22, 97, 77, 69, 6,
		16, 84, 78, 8, 83, 98, 93, 39, 38, 85, 70, 3, 19, 57, 5, 87,
		92, 63, 56
	};
	ITER = 100;
	for(int i = 0; i < ITER; i++){
		perm[i] = nperm[i];
	}
	*/

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	timespecsub(&end, &start, &diff);
	TDEBUGF("done generating a 'random' permutation in: %llu.%09llu s",
	    (unsigned long long)diff.tv_sec, (unsigned long long)diff.tv_nsec);


	for (j = 0; j < 10; j++) {
		TDEBUGF("starting random insertions");
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
		mix_operations(perm, ITER, nodes, ITER, ITER, 0, 0);
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
		timespecsub(&end, &start, &diff);
		TDEBUGF("done random insertions in: %llu.%09llu s",
		(unsigned long long)diff.tv_sec, (unsigned long long)diff.tv_nsec);
	}

	free(nodes);
	free(perm);
	free(nums);

	return 0;
}


#ifdef RB_TEST_DIAGNOSTIC
static void
print_helper(const struct rb_node *rbn, int indent)
{
	struct node *n = rb_entry(rbn, struct node, node_link);
	if (n->node_link.rb_right)
		print_helper(n->node_link.rb_right, indent + 4);
	TDEBUGF("%*s key=%d :: size=%zu :: rank=%d :: rdiff=%lu",
	    indent, "", n->key, n->size, RB_RANK(linux_root, rbn), _RB_GET_RDIFF2(rbn, __entry));
	if (n->node_link.rb_left)
		print_helper(n->node_link.rb_left, indent + 4);
}

static void
print_tree(const struct rb_root_cached *t)
{
	struct rb_node *tree_root = t->rb_root.rb_node;
	if (tree_root) print_helper(tree_root, 0);
}
#endif

#ifdef DOAUGMENT
static int
tree_augment(struct node *elm)
{
	size_t newsize = 1, newheight = 0;
	if ((RB_LEFT(elm, node_link))) {
		newsize += (RB_LEFT(elm, node_link))->size;
		newheight = MAX((RB_LEFT(elm, node_link))->height, newheight);
	}
	if ((RB_RIGHT(elm, node_link))) {
		newsize += (RB_RIGHT(elm, node_link))->size;
		newheight = MAX((RB_RIGHT(elm, node_link))->height, newheight);
	}
	newheight += 1;
	if (elm->size != newsize || elm->height != newheight) {
		elm->size = newsize;
		elm->height = newheight;
		return 1;
	}
	return 0;
}
#endif


void
mix_operations(int *perm, int psize, struct node *nodes, int nsize,
    int insertions, int reads, int do_reads)
{
	int i, rank;
	struct node *tmp, *ins;
	struct node it;
	assert(psize == nsize);
	assert(insertions + reads <= psize);

	for(i = 0; i < insertions; i++) {
		tmp = &(nodes[i]);
		if (tmp == NULL) err(1, "malloc");
		tmp->size = 1;
		tmp->height = 1;
		tmp->key = perm[i];
		insert(tmp, &root);
#ifdef DOAUGMENT
                //TDEBUGF("size = %zu", RB_ROOT(&root)->size);
                assert(RB_ROOT(&root)->size == i + 1);
#endif

#ifdef RB_TEST_RANK
		if (i % RANK_TEST_ITERATIONS == 0) {
			rank = RB_RANK(tree, RB_ROOT(&root));
			if (rank == -2)
				errx(1, "rank error");
		}
#endif
	}
	tmp = &(nodes[insertions]);
	tmp->key = ITER + 5;
	tmp->size = 1;
	tmp->height = 1;
	insert(tmp, &root);
	erase(tmp, &root);

	for(i = 0; i < insertions; i++) {
		tmp = &(nodes[i]);
		erase(tmp, &root);
	}
}
