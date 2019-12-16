/*
 * Copyright (C) 2012-2016  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: rbt_test.c,v 1.1 2019/12/16 16:31:34 deraadt Exp $ */

/* ! \file */

#include <config.h>
#include <atf-c.h>
#include <isc/mem.h>
#include <isc/random.h>
#include <isc/string.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef HAVE_INTTYPES_H
#include <inttypes.h> /* uintptr_t */
#endif

#include <dns/rbt.h>
#include <dns/fixedname.h>
#include <dns/result.h>
#include <dns/compress.h>
#include "dnstest.h"

#include <isc/app.h>
#include <isc/buffer.h>
#include <isc/entropy.h>
#include <isc/file.h>
#include <isc/hash.h>
#include <isc/mem.h>
#include <isc/os.h>
#include <isc/string.h>
#include <isc/socket.h>
#include <isc/stdio.h>
#include <isc/task.h>
#include <isc/thread.h>
#include <isc/timer.h>
#include <isc/util.h>
#include <isc/print.h>
#include <isc/time.h>

#include <dns/log.h>
#include <dns/name.h>
#include <dns/result.h>

#include <dst/dst.h>

#include <ctype.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
	dns_rbt_t *rbt;
	dns_rbt_t *rbt_distances;
} test_context_t;

/* The initial structure of domain tree will be as follows:
 *
 *	       .
 *	       |
 *	       b
 *	     /	 \
 *	    a	 d.e.f
 *		/  |   \
 *	       c   |	g.h
 *		   |	 |
 *		  w.y	 i
 *		/  |  \	  \
 *	       x   |   z   k
 *		   |   |
 *		   p   j
 *		 /   \
 *		o     q
 */

/* The full absolute names of the nodes in the tree (the tree also
 * contains "." which is not included in this list).
 */
static const char * const domain_names[] = {
    "c", "b", "a", "x.d.e.f", "z.d.e.f", "g.h", "i.g.h", "o.w.y.d.e.f",
    "j.z.d.e.f", "p.w.y.d.e.f", "q.w.y.d.e.f", "k.g.h"
};

static const size_t domain_names_count = (sizeof(domain_names) /
					  sizeof(domain_names[0]));

/* These are set as the node data for the tree used in distances check
 * (for the names in domain_names[] above).
 */
static const int node_distances[] = {
    3, 1, 2, 2, 2, 3, 1, 2, 1, 1, 2, 2
};

/*
 * The domain order should be:
 * ., a, b, c, d.e.f, x.d.e.f, w.y.d.e.f, o.w.y.d.e.f, p.w.y.d.e.f,
 * q.w.y.d.e.f, z.d.e.f, j.z.d.e.f, g.h, i.g.h, k.g.h
 *	       . (no data, can't be found)
 *	       |
 *	       b
 *	     /	 \
 *	    a	 d.e.f
 *		/  |   \
 *	       c   |	g.h
 *		   |	 |
 *		  w.y	 i
 *		/  |  \	  \
 *	       x   |   z   k
 *		   |   |
 *		   p   j
 *		 /   \
 *		o     q
 */

static const char * const ordered_names[] = {
    "a", "b", "c", "d.e.f", "x.d.e.f", "w.y.d.e.f", "o.w.y.d.e.f",
    "p.w.y.d.e.f", "q.w.y.d.e.f", "z.d.e.f", "j.z.d.e.f",
    "g.h", "i.g.h", "k.g.h"};

static const size_t ordered_names_count = (sizeof(ordered_names) /
					   sizeof(*ordered_names));

static void
delete_data(void *data, void *arg) {
	UNUSED(arg);

	isc_mem_put(mctx, data, sizeof(size_t));
}

static void
build_name_from_str(const char *namestr, dns_fixedname_t *fname) {
	size_t length;
	isc_buffer_t *b = NULL;
	isc_result_t result;
	dns_name_t *name;

	length = strlen(namestr);

	result = isc_buffer_allocate(mctx, &b, length);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	isc_buffer_putmem(b, (const unsigned char *) namestr, length);

	dns_fixedname_init(fname);
	name = dns_fixedname_name(fname);
	ATF_REQUIRE(name != NULL);
	result = dns_name_fromtext(name, b, dns_rootname, 0, NULL);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	isc_buffer_free(&b);
}

static test_context_t *
test_context_setup(void) {
	test_context_t *ctx;
	isc_result_t result;
	size_t i;

	ctx = isc_mem_get(mctx, sizeof(*ctx));
	ATF_REQUIRE(ctx != NULL);

	ctx->rbt = NULL;
	result = dns_rbt_create(mctx, delete_data, NULL, &ctx->rbt);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	ctx->rbt_distances = NULL;
	result = dns_rbt_create(mctx, delete_data, NULL, &ctx->rbt_distances);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	for (i = 0; i < domain_names_count; i++) {
		size_t *n;
		dns_fixedname_t fname;
		dns_name_t *name;

		build_name_from_str(domain_names[i], &fname);

		name = dns_fixedname_name(&fname);

		n = isc_mem_get(mctx, sizeof(size_t));
		*n = i + 1;
		result = dns_rbt_addname(ctx->rbt, name, n);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

		n = isc_mem_get(mctx, sizeof(size_t));
		*n = node_distances[i];
		result = dns_rbt_addname(ctx->rbt_distances, name, n);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	}

	return (ctx);
}

static void
test_context_teardown(test_context_t *ctx) {
	dns_rbt_destroy(&ctx->rbt);
	dns_rbt_destroy(&ctx->rbt_distances);

	isc_mem_put(mctx, ctx, sizeof(*ctx));
}

/*
 * Walk the tree and ensure that all the test nodes are present.
 */
static void
check_test_data(dns_rbt_t *rbt) {
	dns_fixedname_t fixed;
	isc_result_t result;
	dns_name_t *foundname;
	size_t i;

	dns_fixedname_init(&fixed);
	foundname = dns_fixedname_name(&fixed);

	for (i = 0; i < domain_names_count; i++) {
		dns_fixedname_t fname;
		dns_name_t *name;
		size_t *n;

		build_name_from_str(domain_names[i], &fname);

		name = dns_fixedname_name(&fname);
		n = NULL;
		result = dns_rbt_findname(rbt, name, 0, foundname,
					  (void *) &n);
		ATF_CHECK_EQ(result, ISC_R_SUCCESS);
		ATF_CHECK_EQ(*n, i + 1);
	}
}

ATF_TC(rbt_create);
ATF_TC_HEAD(rbt_create, tc) {
	atf_tc_set_md_var(tc, "descr", "Test the creation of an rbt");
}
ATF_TC_BODY(rbt_create, tc) {
	isc_result_t result;
	test_context_t *ctx;
	isc_boolean_t tree_ok;

	UNUSED(tc);

	isc_mem_debugging = ISC_MEM_DEBUGRECORD;

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	ctx = test_context_setup();

	check_test_data(ctx->rbt);

	tree_ok = dns__rbt_checkproperties(ctx->rbt);
	ATF_CHECK_EQ(tree_ok, ISC_TRUE);

	test_context_teardown(ctx);

	dns_test_end();
}

ATF_TC(rbt_nodecount);
ATF_TC_HEAD(rbt_nodecount, tc) {
	atf_tc_set_md_var(tc, "descr", "Test dns_rbt_nodecount() on a tree");
}
ATF_TC_BODY(rbt_nodecount, tc) {
	isc_result_t result;
	test_context_t *ctx;

	UNUSED(tc);

	isc_mem_debugging = ISC_MEM_DEBUGRECORD;

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	ctx = test_context_setup();

	ATF_CHECK_EQ(15, dns_rbt_nodecount(ctx->rbt));

	test_context_teardown(ctx);

	dns_test_end();
}

ATF_TC(rbtnode_get_distance);
ATF_TC_HEAD(rbtnode_get_distance, tc) {
	atf_tc_set_md_var(tc, "descr",
			  "Test dns_rbtnode_get_distance() on a tree");
}
ATF_TC_BODY(rbtnode_get_distance, tc) {
	isc_result_t result;
	test_context_t *ctx;
	const char *name_str = "a";
	dns_fixedname_t fname;
	dns_name_t *name;
	dns_rbtnode_t *node = NULL;
	dns_rbtnodechain_t chain;

	UNUSED(tc);

	isc_mem_debugging = ISC_MEM_DEBUGRECORD;

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	ctx = test_context_setup();

	build_name_from_str(name_str, &fname);
	name = dns_fixedname_name(&fname);

	dns_rbtnodechain_init(&chain, mctx);

	result = dns_rbt_findnode(ctx->rbt_distances, name, NULL,
				  &node, &chain, 0, NULL, NULL);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	while (node != NULL) {
		const size_t *distance = (const size_t *) node->data;
		if (distance != NULL)
			ATF_CHECK_EQ(*distance,
				     dns__rbtnode_getdistance(node));
		result = dns_rbtnodechain_next(&chain, NULL, NULL);
		if (result == ISC_R_NOMORE)
		      break;
		dns_rbtnodechain_current(&chain, NULL, NULL, &node);
	}

	ATF_CHECK_EQ(result, ISC_R_NOMORE);

	dns_rbtnodechain_invalidate(&chain);

	test_context_teardown(ctx);

	dns_test_end();
}

ATF_TC(rbt_check_distance_random);
ATF_TC_HEAD(rbt_check_distance_random, tc) {
	atf_tc_set_md_var(tc, "descr",
			  "Test tree balance, inserting names in random order");
}
ATF_TC_BODY(rbt_check_distance_random, tc) {
	/* This test checks an important performance-related property of
	 * the red-black tree, which is important for us: the longest
	 * path from a sub-tree's root to a node is no more than
	 * 2log(n). This check verifies that the tree is balanced.
	 */
	dns_rbt_t *mytree = NULL;
	const unsigned int log_num_nodes = 16;

	int i;
	isc_result_t result;
	isc_boolean_t tree_ok;

	UNUSED(tc);

	isc_mem_debugging = ISC_MEM_DEBUGRECORD;

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	result = dns_rbt_create(mctx, delete_data, NULL, &mytree);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	/* Names are inserted in random order. */

	/* Make a large 65536 node top-level domain tree, i.e., the
	 * following code inserts names such as:
	 *
	 * savoucnsrkrqzpkqypbygwoiliawpbmz.
	 * wkadamcbbpjtundbxcmuayuycposvngx.
	 * wzbpznemtooxdpjecdxynsfztvnuyfao.
	 * yueojmhyffslpvfmgyfwioxegfhepnqq.
	 */
	for (i = 0; i < (1 << log_num_nodes); i++) {
		size_t *n;
		char namebuf[34];

		n = isc_mem_get(mctx, sizeof(size_t));
		*n = i + 1;

		while (1) {
			int j;
			dns_fixedname_t fname;
			dns_name_t *name;

			for (j = 0; j < 32; j++) {
				isc_uint32_t v;
				isc_random_get(&v);
				namebuf[j] = 'a' + (v % 26);
			}
			namebuf[32] = '.';
			namebuf[33] = 0;

			build_name_from_str(namebuf, &fname);
			name = dns_fixedname_name(&fname);

			result = dns_rbt_addname(mytree, name, n);
			if (result == ISC_R_SUCCESS)
				break;
		}
	}

	/* 1 (root . node) + (1 << log_num_nodes) */
	ATF_CHECK_EQ(1U + (1U << log_num_nodes), dns_rbt_nodecount(mytree));

	/* The distance from each node to its sub-tree root must be less
	 * than 2 * log(n).
	 */
	ATF_CHECK((2U * log_num_nodes) >= dns__rbt_getheight(mytree));

	/* Also check RB tree properties */
	tree_ok = dns__rbt_checkproperties(mytree);
	ATF_CHECK_EQ(tree_ok, ISC_TRUE);

	dns_rbt_destroy(&mytree);

	dns_test_end();
}

ATF_TC(rbt_check_distance_ordered);
ATF_TC_HEAD(rbt_check_distance_ordered, tc) {
	atf_tc_set_md_var(tc, "descr",
			  "Test tree balance, inserting names in sorted order");
}
ATF_TC_BODY(rbt_check_distance_ordered, tc) {
	/* This test checks an important performance-related property of
	 * the red-black tree, which is important for us: the longest
	 * path from a sub-tree's root to a node is no more than
	 * 2log(n). This check verifies that the tree is balanced.
	 */
	dns_rbt_t *mytree = NULL;
	const unsigned int log_num_nodes = 16;

	int i;
	isc_result_t result;
	isc_boolean_t tree_ok;

	UNUSED(tc);

	isc_mem_debugging = ISC_MEM_DEBUGRECORD;

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	result = dns_rbt_create(mctx, delete_data, NULL, &mytree);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	/* Names are inserted in sorted order. */

	/* Make a large 65536 node top-level domain tree, i.e., the
	 * following code inserts names such as:
	 *
	 *   name00000000.
	 *   name00000001.
	 *   name00000002.
	 *   name00000003.
	 */
	for (i = 0; i < (1 << log_num_nodes); i++) {
		size_t *n;
		char namebuf[14];
		dns_fixedname_t fname;
		dns_name_t *name;

		n = isc_mem_get(mctx, sizeof(size_t));
		*n = i + 1;

		snprintf(namebuf, sizeof(namebuf), "name%08x.", i);
		build_name_from_str(namebuf, &fname);
		name = dns_fixedname_name(&fname);

		result = dns_rbt_addname(mytree, name, n);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	}

	/* 1 (root . node) + (1 << log_num_nodes) */
	ATF_CHECK_EQ(1U + (1U << log_num_nodes), dns_rbt_nodecount(mytree));

	/* The distance from each node to its sub-tree root must be less
	 * than 2 * log(n).
	 */
	ATF_CHECK((2U * log_num_nodes) >= dns__rbt_getheight(mytree));

	/* Also check RB tree properties */
	tree_ok = dns__rbt_checkproperties(mytree);
	ATF_CHECK_EQ(tree_ok, ISC_TRUE);

	dns_rbt_destroy(&mytree);

	dns_test_end();
}

static isc_result_t
insert_helper(dns_rbt_t *rbt, const char *namestr, dns_rbtnode_t **node) {
	dns_fixedname_t fname;
	dns_name_t *name;

	build_name_from_str(namestr, &fname);
	name = dns_fixedname_name(&fname);

	return (dns_rbt_addnode(rbt, name, node));
}

static isc_boolean_t
compare_labelsequences(dns_rbtnode_t *node, const char *labelstr) {
	dns_name_t name;
	isc_result_t result;
	char *nodestr = NULL;
	isc_boolean_t is_equal;

	dns_name_init(&name, NULL);
	dns_rbt_namefromnode(node, &name);

	result = dns_name_tostring(&name, &nodestr, mctx);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	is_equal = strcmp(labelstr, nodestr) == 0 ? ISC_TRUE : ISC_FALSE;

	isc_mem_free(mctx, nodestr);

	return (is_equal);
}

ATF_TC(rbt_insert);
ATF_TC_HEAD(rbt_insert, tc) {
	atf_tc_set_md_var(tc, "descr", "Test insertion into a tree");
}
ATF_TC_BODY(rbt_insert, tc) {
	isc_result_t result;
	test_context_t *ctx;
	dns_rbtnode_t *node;

	UNUSED(tc);

	isc_mem_debugging = ISC_MEM_DEBUGRECORD;

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	ctx = test_context_setup();

	/* Check node count before beginning. */
	ATF_CHECK_EQ(15, dns_rbt_nodecount(ctx->rbt));

	/* Try to insert a node that already exists. */
	node = NULL;
	result = insert_helper(ctx->rbt, "d.e.f", &node);
	ATF_CHECK_EQ(result, ISC_R_EXISTS);

	/* Node count must not have changed. */
	ATF_CHECK_EQ(15, dns_rbt_nodecount(ctx->rbt));

	/* Try to insert a node that doesn't exist. */
	node = NULL;
	result = insert_helper(ctx->rbt, "0", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	ATF_CHECK_EQ(compare_labelsequences(node, "0"), ISC_TRUE);

	/* Node count must have increased. */
	ATF_CHECK_EQ(16, dns_rbt_nodecount(ctx->rbt));

	/* Another. */
	node = NULL;
	result = insert_helper(ctx->rbt, "example.com", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE(node != NULL);
	ATF_CHECK_EQ(node->data, NULL);

	/* Node count must have increased. */
	ATF_CHECK_EQ(17, dns_rbt_nodecount(ctx->rbt));

	/* Re-adding it should return EXISTS */
	node = NULL;
	result = insert_helper(ctx->rbt, "example.com", &node);
	ATF_CHECK_EQ(result, ISC_R_EXISTS);

	/* Node count must not have changed. */
	ATF_CHECK_EQ(17, dns_rbt_nodecount(ctx->rbt));

	/* Fission the node d.e.f */
	node = NULL;
	result = insert_helper(ctx->rbt, "k.e.f", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	ATF_CHECK_EQ(compare_labelsequences(node, "k"), ISC_TRUE);

	/* Node count must have incremented twice ("d.e.f" fissioned to
	 * "d" and "e.f", and the newly added "k").
	 */
	ATF_CHECK_EQ(19, dns_rbt_nodecount(ctx->rbt));

	/* Fission the node "g.h" */
	node = NULL;
	result = insert_helper(ctx->rbt, "h", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	ATF_CHECK_EQ(compare_labelsequences(node, "h"), ISC_TRUE);

	/* Node count must have incremented ("g.h" fissioned to "g" and
	 * "h").
	 */
	ATF_CHECK_EQ(20, dns_rbt_nodecount(ctx->rbt));

	/* Add child domains */

	node = NULL;
	result = insert_helper(ctx->rbt, "m.p.w.y.d.e.f", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	ATF_CHECK_EQ(compare_labelsequences(node, "m"), ISC_TRUE);
	ATF_CHECK_EQ(21, dns_rbt_nodecount(ctx->rbt));

	node = NULL;
	result = insert_helper(ctx->rbt, "n.p.w.y.d.e.f", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	ATF_CHECK_EQ(compare_labelsequences(node, "n"), ISC_TRUE);
	ATF_CHECK_EQ(22, dns_rbt_nodecount(ctx->rbt));

	node = NULL;
	result = insert_helper(ctx->rbt, "l.a", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	ATF_CHECK_EQ(compare_labelsequences(node, "l"), ISC_TRUE);
	ATF_CHECK_EQ(23, dns_rbt_nodecount(ctx->rbt));

	node = NULL;
	result = insert_helper(ctx->rbt, "r.d.e.f", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	node = NULL;
	result = insert_helper(ctx->rbt, "s.d.e.f", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	ATF_CHECK_EQ(25, dns_rbt_nodecount(ctx->rbt));

	node = NULL;
	result = insert_helper(ctx->rbt, "h.w.y.d.e.f", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	/* Add more nodes one by one to cover left and right rotation
	 * functions.
	 */
	node = NULL;
	result = insert_helper(ctx->rbt, "f", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	node = NULL;
	result = insert_helper(ctx->rbt, "m", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	node = NULL;
	result = insert_helper(ctx->rbt, "nm", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	node = NULL;
	result = insert_helper(ctx->rbt, "om", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	node = NULL;
	result = insert_helper(ctx->rbt, "k", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	node = NULL;
	result = insert_helper(ctx->rbt, "l", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	node = NULL;
	result = insert_helper(ctx->rbt, "fe", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	node = NULL;
	result = insert_helper(ctx->rbt, "ge", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	node = NULL;
	result = insert_helper(ctx->rbt, "i", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	node = NULL;
	result = insert_helper(ctx->rbt, "ae", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	node = NULL;
	result = insert_helper(ctx->rbt, "n", &node);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	test_context_teardown(ctx);

	dns_test_end();
}

ATF_TC(rbt_remove);
ATF_TC_HEAD(rbt_remove, tc) {
	atf_tc_set_md_var(tc, "descr", "Test removal from a tree");
}
ATF_TC_BODY(rbt_remove, tc) {
	/*
	 * This testcase checks that after node removal, the
	 * binary-search tree is valid and all nodes that are supposed
	 * to exist are present in the correct order. It mainly tests
	 * DomainTree as a BST, and not particularly as a red-black
	 * tree. This test checks node deletion when upper nodes have
	 * data.
	 */
	isc_result_t result;
	size_t j;

	UNUSED(tc);

	isc_mem_debugging = ISC_MEM_DEBUGRECORD;

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	/*
	 * Delete single nodes and check if the rest of the nodes exist.
	 */
	for (j = 0; j < ordered_names_count; j++) {
		dns_rbt_t *mytree = NULL;
		dns_rbtnode_t *node;
		size_t i;
		size_t *n;
		isc_boolean_t tree_ok;
		dns_rbtnodechain_t chain;
		size_t start_node;

		/* Create a tree. */
		result = dns_rbt_create(mctx, delete_data, NULL, &mytree);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

		/* Insert test data into the tree. */
		for (i = 0; i < domain_names_count; i++) {
			node = NULL;
			result = insert_helper(mytree, domain_names[i], &node);
			ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		}

		/* Check that all names exist in order. */
		for (i = 0; i < ordered_names_count; i++) {
			dns_fixedname_t fname;
			dns_name_t *name;

			build_name_from_str(ordered_names[i], &fname);

			name = dns_fixedname_name(&fname);
			node = NULL;
			result = dns_rbt_findnode(mytree, name, NULL,
						  &node, NULL,
						  DNS_RBTFIND_EMPTYDATA,
						  NULL, NULL);
			ATF_CHECK_EQ(result, ISC_R_SUCCESS);

			/* Add node data */
			ATF_REQUIRE(node != NULL);
			ATF_REQUIRE_EQ(node->data, NULL);

			n = isc_mem_get(mctx, sizeof(size_t));
			*n = i;

			node->data = n;
		}

		/* Now, delete the j'th node from the tree. */
		{
			dns_fixedname_t fname;
			dns_name_t *name;

			build_name_from_str(ordered_names[j], &fname);

			name = dns_fixedname_name(&fname);

			result = dns_rbt_deletename(mytree, name, ISC_FALSE);
			ATF_CHECK_EQ(result, ISC_R_SUCCESS);
		}

		/* Check RB tree properties. */
		tree_ok = dns__rbt_checkproperties(mytree);
		ATF_CHECK_EQ(tree_ok, ISC_TRUE);

		dns_rbtnodechain_init(&chain, mctx);

		/* Now, walk through nodes in order. */
		if (j == 0) {
			/*
			 * Node for ordered_names[0] was already deleted
			 * above. We start from node 1.
			 */
			dns_fixedname_t fname;
			dns_name_t *name;

			build_name_from_str(ordered_names[0], &fname);
			name = dns_fixedname_name(&fname);
			node = NULL;
			result = dns_rbt_findnode(mytree, name, NULL,
						  &node, NULL,
						  0,
						  NULL, NULL);
			ATF_CHECK_EQ(result, ISC_R_NOTFOUND);

			build_name_from_str(ordered_names[1], &fname);
			name = dns_fixedname_name(&fname);
			node = NULL;
			result = dns_rbt_findnode(mytree, name, NULL,
						  &node, &chain,
						  0,
						  NULL, NULL);
			ATF_CHECK_EQ(result, ISC_R_SUCCESS);
			start_node = 1;
		} else {
			/* Start from node 0. */
			dns_fixedname_t fname;
			dns_name_t *name;

			build_name_from_str(ordered_names[0], &fname);
			name = dns_fixedname_name(&fname);
			node = NULL;
			result = dns_rbt_findnode(mytree, name, NULL,
						  &node, &chain,
						  0,
						  NULL, NULL);
			ATF_CHECK_EQ(result, ISC_R_SUCCESS);
			start_node = 0;
		}

		/*
		 * node and chain have been set by the code above at
		 * this point.
		 */
		for (i = start_node; i < ordered_names_count; i++) {
			dns_fixedname_t fname_j, fname_i;
			dns_name_t *name_j, *name_i;

			build_name_from_str(ordered_names[j], &fname_j);
			name_j = dns_fixedname_name(&fname_j);
			build_name_from_str(ordered_names[i], &fname_i);
			name_i = dns_fixedname_name(&fname_i);

			if (dns_name_equal(name_i, name_j)) {
				/*
				 * This may be true for the last node if
				 * we seek ahead in the loop using
				 * dns_rbtnodechain_next() below.
				 */
				if (node == NULL) {
					break;
				}

				/* All ordered nodes have data
				 * initially. If any node is empty, it
				 * means it was removed, but an empty
				 * node exists because it is a
				 * super-domain. Just skip it.
				 */
				if (node->data == NULL) {
					result = dns_rbtnodechain_next(&chain,
								       NULL,
								       NULL);
					if (result == ISC_R_NOMORE) {
						node = NULL;
					} else {
						dns_rbtnodechain_current(&chain,
									 NULL,
									 NULL,
									 &node);
					}
				}
				continue;
			}

			ATF_REQUIRE(node != NULL);

			n = (size_t *) node->data;
			if (n != NULL) {
				/* printf("n=%zu, i=%zu\n", *n, i); */
				ATF_CHECK_EQ(*n, i);
			}

			result = dns_rbtnodechain_next(&chain, NULL, NULL);
			if (result == ISC_R_NOMORE) {
				node = NULL;
			} else {
				dns_rbtnodechain_current(&chain, NULL, NULL,
							 &node);
			}
		}

		/* We should have reached the end of the tree. */
		ATF_REQUIRE_EQ(node, NULL);

		dns_rbt_destroy(&mytree);
	}

	dns_test_end();
}

static void
insert_nodes(dns_rbt_t *mytree, char **names,
	     size_t *names_count, isc_uint32_t num_names)
{
	isc_uint32_t i;

	for (i = 0; i < num_names; i++) {
		size_t *n;
		char namebuf[34];

		n = isc_mem_get(mctx, sizeof(size_t));
		ATF_REQUIRE(n != NULL);

		*n = i; /* Unused value */

		while (1) {
			int j;
			dns_fixedname_t fname;
			dns_name_t *name;
			isc_result_t result;

			for (j = 0; j < 32; j++) {
				isc_uint32_t v;
				isc_random_get(&v);
				namebuf[j] = 'a' + (v % 26);
			}
			namebuf[32] = '.';
			namebuf[33] = 0;

			build_name_from_str(namebuf, &fname);
			name = dns_fixedname_name(&fname);

			result = dns_rbt_addname(mytree, name, n);
			if (result == ISC_R_SUCCESS) {
				names[*names_count] = isc_mem_strdup(mctx,
								     namebuf);
				*names_count += 1;
				break;
			}
		}
	}
}

static void
remove_nodes(dns_rbt_t *mytree, char **names,
	     size_t *names_count, isc_uint32_t num_names)
{
	isc_uint32_t i;

	UNUSED(mytree);

	for (i = 0; i < num_names; i++) {
		isc_uint32_t node;
		dns_fixedname_t fname;
		dns_name_t *name;
		isc_result_t result;

		isc_random_get(&node);

		node %= *names_count;

		build_name_from_str(names[node], &fname);
		name = dns_fixedname_name(&fname);

		result = dns_rbt_deletename(mytree, name, ISC_FALSE);
		ATF_CHECK_EQ(result, ISC_R_SUCCESS);

		isc_mem_free(mctx, names[node]);
		if (*names_count > 0) {
			names[node] = names[*names_count - 1];
			names[*names_count - 1] = NULL;
			*names_count -= 1;
		}
	}
}

static void
check_tree(dns_rbt_t *mytree, char **names, size_t names_count) {
	isc_boolean_t tree_ok;

	UNUSED(names);

	ATF_CHECK_EQ(names_count + 1, dns_rbt_nodecount(mytree));

	/*
	 * The distance from each node to its sub-tree root must be less
	 * than 2 * log_2(1024).
	 */
	ATF_CHECK((2 * 10) >= dns__rbt_getheight(mytree));

	/* Also check RB tree properties */
	tree_ok = dns__rbt_checkproperties(mytree);
	ATF_CHECK_EQ(tree_ok, ISC_TRUE);
}

ATF_TC(rbt_insert_and_remove);
ATF_TC_HEAD(rbt_insert_and_remove, tc) {
	atf_tc_set_md_var(tc, "descr",
			  "Test insert and remove in a loop");
}
ATF_TC_BODY(rbt_insert_and_remove, tc) {
	/*
	 * What is the best way to test our red-black tree code? It is
	 * not a good method to test every case handled in the actual
	 * code itself. This is because our approach itself may be
	 * incorrect.
	 *
	 * We test our code at the interface level here by exercising the
	 * tree randomly multiple times, checking that red-black tree
	 * properties are valid, and all the nodes that are supposed to be
	 * in the tree exist and are in order.
	 *
	 * NOTE: These tests are run within a single tree level in the
	 * forest. The number of nodes in the tree level doesn't grow
	 * over 1024.
	 */
	isc_result_t result;
	dns_rbt_t *mytree = NULL;
	/*
	 * We use an array for storing names instead of a set
	 * structure. It's slow, but works and is good enough for tests.
	 */
	char *names[1024];
	size_t names_count;
	int i;

	UNUSED(tc);

	isc_mem_debugging = ISC_MEM_DEBUGRECORD;

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);

	result = dns_rbt_create(mctx, delete_data, NULL, &mytree);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	memset(names, 0, sizeof(names));
	names_count = 0;

	/* Repeat the insert/remove test some 4096 times */
	for (i = 0; i < 4096; i++) {
		isc_uint32_t num_names;
		isc_random_get(&num_names);

		if (names_count < 1024) {
			num_names %= 1024 - names_count;
			num_names++;
		} else {
			num_names = 0;
		}

		insert_nodes(mytree, names, &names_count, num_names);
		check_tree(mytree, names, names_count);

		isc_random_get(&num_names);
		if (names_count > 0) {
			num_names %= names_count;
			num_names++;
		} else {
			num_names = 0;
		}

		remove_nodes(mytree, names, &names_count, num_names);
		check_tree(mytree, names, names_count);
	}

	/* Remove the rest of the nodes */
	remove_nodes(mytree, names, &names_count, names_count);
	check_tree(mytree, names, names_count);

	for (i = 0; i < 1024; i++) {
		if (names[i] != NULL) {
			isc_mem_free(mctx, names[i]);
		}
	}

	dns_rbt_destroy(&mytree);

	dns_test_end();
}

#ifdef ISC_PLATFORM_USETHREADS
#ifdef DNS_BENCHMARK_TESTS

/*
 * XXXMUKS: Don't delete this code. It is useful in benchmarking the
 * RBT, but we don't require it as part of the unit test runs.
 */

ATF_TC(benchmark);
ATF_TC_HEAD(benchmark, tc) {
	atf_tc_set_md_var(tc, "descr", "Benchmark RBT implementation");
}

static dns_fixedname_t *fnames;
static dns_name_t **names;
static int *values;

static void *
find_thread(void *arg) {
	dns_rbt_t *mytree;
	isc_result_t result;
	dns_rbtnode_t *node;
	unsigned int j, i;
	unsigned int start = 0;

	mytree = (dns_rbt_t *) arg;
	while (start == 0)
		start = random() % 4000000;

	/* Query 32 million random names from it in each thread */
	for (j = 0; j < 8; j++) {
		for (i = start; i != start - 1; i = (i + 1) % 4000000) {
			node = NULL;
			result = dns_rbt_findnode(mytree, names[i], NULL,
						  &node, NULL,
						  DNS_RBTFIND_EMPTYDATA,
						  NULL, NULL);
			ATF_CHECK_EQ(result, ISC_R_SUCCESS);
			ATF_REQUIRE(node != NULL);
			ATF_CHECK_EQ(values[i], (intptr_t) node->data);
		}
	}

	return (NULL);
}

ATF_TC_BODY(benchmark, tc) {
	isc_result_t result;
	char namestr[sizeof("name18446744073709551616.example.org.")];
	unsigned int r;
	dns_rbt_t *mytree;
	dns_rbtnode_t *node;
	unsigned int i;
	unsigned int maxvalue = 1000000;
	isc_time_t ts1, ts2;
	double t;
	unsigned int nthreads;
	isc_thread_t threads[32];

	UNUSED(tc);

	srandom(time(NULL));

	debug_mem_record = ISC_FALSE;

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	fnames = (dns_fixedname_t *) malloc(4000000 * sizeof(dns_fixedname_t));
	names = (dns_name_t **) malloc(4000000 * sizeof(dns_name_t *));
	values = (int *) malloc(4000000 * sizeof(int));

	for (i = 0; i < 4000000; i++) {
		  r = ((unsigned long) random()) % maxvalue;
		  snprintf(namestr, sizeof(namestr), "name%u.example.org.", r);
		  build_name_from_str(namestr, &fnames[i]);
		  names[i] = dns_fixedname_name(&fnames[i]);
		  values[i] = r;
	}

	/* Create a tree. */
	mytree = NULL;
	result = dns_rbt_create(mctx, NULL, NULL, &mytree);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	/* Insert test data into the tree. */
	for (i = 0; i < maxvalue; i++) {
		snprintf(namestr, sizeof(namestr), "name%u.example.org.", i);
		node = NULL;
		result = insert_helper(mytree, namestr, &node);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		node->data = (void *) (intptr_t) i;
	}

	result = isc_time_now(&ts1);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	nthreads = ISC_MIN(isc_os_ncpus(), 32);
	nthreads = ISC_MAX(nthreads, 1);
	for (i = 0; i < nthreads; i++) {
		result = isc_thread_create(find_thread, mytree, &threads[i]);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	}

	for (i = 0; i < nthreads; i++) {
		result = isc_thread_join(threads[i], NULL);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	}

	result = isc_time_now(&ts2);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	t = isc_time_microdiff(&ts2, &ts1);

	printf("%u findnode calls, %f seconds, %f calls/second\n",
	       nthreads * 8 * 4000000, t / 1000000.0,
	       (nthreads * 8 * 4000000) / (t / 1000000.0));

	free(values);
	free(names);
	free(fnames);

	dns_rbt_destroy(&mytree);

	dns_test_end();
}

#endif /* DNS_BENCHMARK_TESTS */
#endif /* ISC_PLATFORM_USETHREADS */

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, rbt_create);
	ATF_TP_ADD_TC(tp, rbt_nodecount);
	ATF_TP_ADD_TC(tp, rbtnode_get_distance);
	ATF_TP_ADD_TC(tp, rbt_check_distance_random);
	ATF_TP_ADD_TC(tp, rbt_check_distance_ordered);
	ATF_TP_ADD_TC(tp, rbt_insert);
	ATF_TP_ADD_TC(tp, rbt_remove);
	ATF_TP_ADD_TC(tp, rbt_insert_and_remove);
#ifdef ISC_PLATFORM_USETHREADS
#ifdef DNS_BENCHMARK_TESTS
	ATF_TP_ADD_TC(tp, benchmark);
#endif /* DNS_BENCHMARK_TESTS */
#endif /* ISC_PLATFORM_USETHREADS */

	return (atf_no_error());
}
