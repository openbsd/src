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
