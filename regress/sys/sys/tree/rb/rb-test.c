/*	$OpenBSD: rb-test.c,v 1.2 2002/10/16 15:30:06 art Exp $	*/
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
#include <sys/types.h>
#include <sys/tree.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>
#include <stdlib.h>

struct node {
	RB_ENTRY(node) node;
	int key;
};

RB_HEAD(tree, node) root;

int
compare(struct node *a, struct node *b)
{
	if (a->key < b->key) return (-1);
	else if (a->key > b->key) return (1);
	return (0);
}

RB_PROTOTYPE(tree, node, node, compare);

RB_GENERATE(tree, node, node, compare);

#define ITER 150
#define MIN 5
#define MAX 5000

int
main(int argc, char **argv)
{
	struct node *tmp, *ins;
	int i, max, min;

	RB_INIT(&root);

	for (i = 0; i < ITER; i++) {
		tmp = malloc(sizeof(struct node));
		if (tmp == NULL) err(1, "malloc");
		do {
			tmp->key = arc4random() % (MAX-MIN);
			tmp->key += MIN;
		} while (RB_FIND(tree, &root, tmp) != NULL);
		if (i == 0)
			max = min = tmp->key;
		else {
			if (tmp->key > max)
				max = tmp->key;
			if (tmp->key < min)
				min = tmp->key;
		}
		if (RB_INSERT(tree, &root, tmp) != NULL)
			errx(1, "RB_INSERT failed");
	}

	ins = RB_MIN(tree, &root);
	if (ins->key != min)
		errx(1, "min does not match");
	tmp = ins;
	ins = RB_MAX(tree, &root);
	if (ins->key != max)
		errx(1, "max does not match");

	if (RB_REMOVE(tree, &root, tmp) != tmp)
		errx(1, "RB_REMOVE failed");

	for (i = 0; i < ITER - 1; i++) {
		tmp = RB_ROOT(&root);
		if (tmp == NULL)
			errx(1, "RB_ROOT error");
		if (RB_REMOVE(tree, &root, tmp) != tmp)
			errx(1, "RB_REMOVE error");
		free(tmp);
	}

	exit(0);
}
