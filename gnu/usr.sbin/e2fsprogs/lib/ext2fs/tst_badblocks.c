/*
 * This testing program makes sure the badblocks implementation works.
 *
 * Copyright (C) 1996 by Theodore Ts'o.
 * 
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include <linux/ext2_fs.h>

#include "ext2fs.h"

blk_t test1[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0 };
blk_t test2[] = { 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
blk_t test3[] = { 3, 1, 4, 5, 9, 2, 7, 10, 6, 8, 0 };
blk_t test4[] = { 20, 50, 12, 17, 13, 2, 66, 23, 56, 0 };
blk_t test4a[] = {
 	20, 1,
	50, 1,
	3, 0,
	17, 1,
	18, 0,
	16, 0,
	11, 0,
	12, 1,
	13, 1,
	14, 0, 
	80, 0,
	45, 0,
	66, 1,
	0 };

static int test_fail = 0;

static errcode_t create_test_list(blk_t *vec, badblocks_list *ret)
{
	errcode_t	retval;
	badblocks_list	bb;
	int		i;
	
	retval = badblocks_list_create(&bb, 5);
	if (retval) {
		com_err("create_test_list", retval, "while creating list");
		return retval;
	}
	for (i=0; vec[i]; i++) {
		retval = badblocks_list_add(bb, vec[i]);
		if (retval) {
			com_err("create_test_list", retval,
				"while adding test vector %d", i);
			badblocks_list_free(bb);
			return retval;
		}
	}
	*ret = bb;
	return 0;
}

static void print_list(badblocks_list bb, int verify)
{
	errcode_t	retval;
	badblocks_iterate	iter;
	blk_t			blk;
	int			i, ok;
	
	retval = badblocks_list_iterate_begin(bb, &iter);
	if (retval) {
		com_err("print_list", retval, "while setting up iterator");
		return;
	}
	ok = i = 1;
	while (badblocks_list_iterate(iter, &blk)) {
		printf("%d ", blk);
		if (i++ != blk)
			ok = 0;
	}
	badblocks_list_iterate_end(iter);
	if (verify) {
		if (ok)
			printf("--- OK");
		else {
			printf("--- NOT OK");
			test_fail++;
		}
	}
}

static void validate_test_seq(badblocks_list bb, blk_t *vec)
{
	int	i, match, ok;

	for (i = 0; vec[i]; i += 2) {
		match = badblocks_list_test(bb, vec[i]);
		if (match == vec[i+1])
			ok = 1;
		else {
			ok = 0;
			test_fail++;
		}
		printf("\tblock %d is %s --- %s\n", vec[i],
		       match ? "present" : "absent",
		       ok ? "OK" : "NOT OK");
	}
}

int main(int argc, char *argv)
{
	badblocks_list bb;
	errcode_t	retval;

	printf("test1: ");
	retval = create_test_list(test1, &bb);
	if (retval == 0) {
		print_list(bb, 1);
		badblocks_list_free(bb);
	}
	printf("\n");
	
	printf("test2: ");
	retval = create_test_list(test2, &bb);
	if (retval == 0) {
		print_list(bb, 1);
		badblocks_list_free(bb);
	}
	printf("\n");

	printf("test3: ");
	retval = create_test_list(test3, &bb);
	if (retval == 0) {
		print_list(bb, 1);
		badblocks_list_free(bb);
	}
	printf("\n");
	
	printf("test4: ");
	retval = create_test_list(test4, &bb);
	if (retval == 0) {
		print_list(bb, 0);
		printf("\n");
		validate_test_seq(bb, test4a);
		badblocks_list_free(bb);
	}
	printf("\n");
	if (test_fail == 0)
		printf("ext2fs library badblocks tests checks out OK!\n");
	return test_fail;
}
