/*	$OpenBSD: fdt.c,v 1.8 2010/04/20 23:35:16 deraadt Exp $	*/

/*
 * Copyright (c) 2009 Dariusz Swiderski <sfires@sfires.net>
 * Copyright (c) 2009 Mark Kettenis <kettenis@sfires.net>
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


#include <sys/types.h>
#include <sys/param.h>

#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>

unsigned int fdt_check_head(void *);
char	*fdt_get_str(u_int32_t);
void	*skip_property(u_int32_t *);
void	*skip_props(u_int32_t *);
void	*skip_node_name(u_int32_t *);
void	*fdt_parent_node_recurse(void *, void *);
#ifdef DEBUG
void 	 fdt_print_node_recurse(void *, int);
#endif

static int tree_inited = 0;
static struct fdt tree;

unsigned int
fdt_check_head(void *fdt)
{
	struct fdt_head *fh;
	u_int32_t *ptr;

	fh = fdt;
	ptr = (u_int32_t *)fdt;

	if (fh->fh_magic != FDT_MAGIC)
		return 0;

	if (fh->fh_version > FDT_CODE_VERSION)
		return 0;

	if (*(ptr + (fh->fh_struct_off / 4)) != FDT_NODE_BEGIN)
		return 0;

	/* check for end signature on version 17 blob */
	if ((fh->fh_version >= 17) & (*(ptr + fh->fh_struct_size) != FDT_END))
		return 0;

	return fh->fh_version;
}

/*
 * Initializes internal structures of module.
 * Has to be called once, preferably in machdep.c.
 */
int
fdt_init(void *fdt)
{
	int version;

	bzero(&tree, sizeof(struct fdt));
	tree_inited = 0;

	if (!fdt)
		return 0;

	if (!(version = fdt_check_head(fdt)))
		return 0;

	tree.header = (struct fdt_head *)fdt;
	tree.tree = (char *)fdt + tree.header->fh_struct_off;
	tree.strings = (char *)fdt + tree.header->fh_strings_off;
	tree.memory = (char *)fdt + tree.header->fh_reserve_off;
	tree.version = version;

	if (version < 3) {
		if ((tree.strings < tree.tree) && (tree.tree < tree.memory))
			tree.strings_size = tree.tree - tree.strings;
		else if ((tree.strings < tree.memory) && (tree.memory <
		    tree.tree))
			tree.strings_size = tree.memory - tree.strings;
		else if ((tree.strings < tree.tree) && (tree.memory <
		    tree.strings))
			tree.strings_size = tree.tree - tree.strings;
		else if ((tree.strings < tree.memory) && (tree.tree <
		    tree.strings))
			tree.strings_size = tree.memory - tree.strings;
		else
			tree.strings_size = tree.header->fh_size -
			    (int)tree.strings;
	} else
		tree.strings_size = tree.header->fh_strings_size;

	tree.strings_size = tree.header->fh_strings_size;
	tree_inited = 1;

	return version;
}

/*
 * Retrieve string pointer from strings table.
 */
char *
fdt_get_str(u_int32_t num)
{
	if (num > tree.strings_size)
		return NULL;
	return (tree.strings) ? (tree.strings + num) : NULL;
}

/*
 * Utility functions for skipping parts of tree.
 */
void *
skip_property(u_int32_t *ptr)
{
	u_int32_t size;

	size = *(ptr + 1);
	/* move forward by magic + size + nameid + rounded up property size */
	ptr += 3 + roundup(size, sizeof(u_int32_t)) / sizeof(u_int32_t);

	return ptr;
}

void *
skip_props(u_int32_t *ptr)
{
	while (*ptr == FDT_PROPERTY) {
		ptr = skip_property(ptr);
	}
	return ptr;
}

void *
skip_node_name(u_int32_t *ptr)
{
	/* skip name, aligned to 4 bytes, this is NULL term., so must add 1 */
	return ptr + roundup(strlen((char *)ptr) + 1,
	    sizeof(u_int32_t)) / sizeof(u_int32_t);
}

/*
 * Retrieves node property, the returned pointer is inside the fdt tree,
 * so we should not modify content pointed by it directly.
 */
int
fdt_node_property(void *node, char *name, char **out)
{
	u_int32_t *ptr;
	u_int32_t nameid;
	char *tmp;
	
	if (!tree_inited)
		return 0;

	ptr = (u_int32_t *)node;

	if (*ptr != FDT_NODE_BEGIN)
		return 0;

	ptr = skip_node_name(ptr + 1);

	while (*ptr == FDT_PROPERTY) {
		nameid = *(ptr + 2); /* id of name in strings table */
		tmp = fdt_get_str(nameid);
		if (!strcmp(name, tmp)) {
			*out = (char *)(ptr + 3); /* begining of the value */
			return *(ptr + 1); /* size of value */
		}
		ptr = skip_property(ptr);
	}
	return 0;
}

/*
 * Retrieves next node, skipping all the children nodes of the pointed node
 * if passed 0 wil return first node of the tree (root)
 */
void *
fdt_next_node(void *node)
{
	u_int32_t *ptr;

	if (!tree_inited)
		return NULL;

	ptr = node;

	if (!node) {
		ptr = tree.tree;
		return (*ptr == FDT_NODE_BEGIN) ? ptr : NULL;
	}

	if (*ptr != FDT_NODE_BEGIN)
		return NULL;

	ptr++;

	ptr = skip_node_name(ptr);
	ptr = skip_props(ptr);

	/* skip children */
	while (*ptr == FDT_NODE_BEGIN)
		ptr = fdt_next_node(ptr);

	return (*ptr == FDT_NODE_END) ? (ptr + 1) : NULL;
}

/*
 * Retrieves next node, skipping all the children nodes of the pointed node
 */
void *
fdt_child_node(void *node)
{
	u_int32_t *ptr;

	if (!tree_inited)
		return NULL;

	ptr = node;

	if (*ptr != FDT_NODE_BEGIN)
		return NULL;

	ptr++;

	ptr = skip_node_name(ptr);
	ptr = skip_props(ptr);
	/* check if there is a child node */
	return (*ptr == FDT_NODE_BEGIN) ? (ptr) : NULL;
}

/*
 * Retrieves node name.
 */
char *
fdt_node_name(void *node)
{
	u_int32_t *ptr;

	if (!tree_inited)
		return NULL;

	ptr = node;

	if (*ptr != FDT_NODE_BEGIN)
		return NULL;

	return (char *)(ptr + 1);
}

void *
fdt_find_node(char *name)
{
	void *node = fdt_next_node(0);
	const char *p = name;

	if (!tree_inited)
		return NULL;

	if (*p != '/')
		return NULL;

	while (*p) {
		void *child;
		const char *q;

		while (*p == '/')
			p++;
		if (*p == 0)
			return node;
		q = strchr(p, '/');
		if (q == NULL)
			q = p + strlen(p);

		for (child = fdt_child_node(node); child;
		     child = fdt_next_node(child)) {
			if (strncmp(p, fdt_node_name(child), q - p) == 0) {
				node = child;
				break;
			}
		}

		p = q;
	}

	return node;
}

void *
fdt_parent_node_recurse(void *pnode, void *child)
{
	void *node = fdt_child_node(pnode);
	void *tmp;
	
	while (node && (node != child)) {
		if ((tmp = fdt_parent_node_recurse(node, child)))
			return tmp;
		node = fdt_next_node(node);
	}
	return (node) ? pnode : NULL;
}

void *
fdt_parent_node(void *node)
{
	void *pnode = fdt_next_node(0);

	if (!tree_inited)
		return NULL;

	return fdt_parent_node_recurse(pnode, node);
}

#ifdef DEBUG
/*
 * Debug methods for printing whole tree, particular odes and properies
 */
void *
fdt_print_property(void *node, int level)
{
	u_int32_t *ptr;
	char *tmp, *value;
	int cnt;
	u_int32_t nameid, size;

	ptr = (u_int32_t *)node;

	if (!tree_inited)
		return NULL;

	if (*ptr != FDT_PROPERTY)
		return ptr; /* should never happen */

	/* extract property name_id and size */
	size = *++ptr;
	nameid = *++ptr;

	for (cnt = 0; cnt < level; cnt++)
		printf("\t");

	tmp = fdt_get_str(nameid);
	printf("\t%s : ", tmp ? tmp : "NO_NAME");

	ptr++;
	value = (char *)ptr;

	if (!strcmp(tmp, "device_type") || !strcmp(tmp, "compatible") ||
	    !strcmp(tmp, "model") || !strcmp(tmp, "bootargs") ||
	    !strcmp(tmp, "linux,stdout-path")) {
		printf("%s", value);
	} else if (!strcmp(tmp, "clock-frequency") ||
	    !strcmp(tmp, "timebase-frequency")) {
		printf("%d", *((unsigned int *)value));
	} else {
		for (cnt = 0; cnt < size; cnt++) {
			if ((cnt % sizeof(u_int32_t)) == 0)
				printf(" ");
			printf("%02x", value[cnt]);
		}
	}
	ptr += roundup(size, sizeof(u_int32_t)) / sizeof(u_int32_t);
	printf("\n");

	return ptr;
}

void
fdt_print_node(void *node, int level)
{
	u_int32_t *ptr;
	int cnt;
	
	ptr = (u_int32_t *)node;

	if (*ptr != FDT_NODE_BEGIN)
		return;

	ptr++;

	for (cnt = 0; cnt < level; cnt++)
		printf("\t");
	printf("%s :\n", fdt_node_name(node));
	ptr = skip_node_name(ptr);

	while (*ptr == FDT_PROPERTY)
		ptr = fdt_print_property(ptr, level);
}

void
fdt_print_node_recurse(void *node, int level)
{
	void *child;

	fdt_print_node(node, level);
	for (child = fdt_child_node(node); child; child = fdt_next_node(child))
		fdt_print_node_recurse(child, level + 1);
}

void
fdt_print_tree(void)
{
	fdt_print_node_recurse(fdt_next_node(0), 0);
}
#endif

int
OF_peer(int handle)
{
	void *node = (char *)tree.header + handle;

	if (handle == 0)
		node = fdt_find_node("/");
	else
		node = fdt_next_node(node);
	return node ? ((char *)node - (char *)tree.header) : 0;
}

int
OF_child(int handle)
{
	void *node = (char *)tree.header + handle;

	node = fdt_child_node(node);
	return node ? ((char *)node - (char *)tree.header) : 0;
}

int
OF_parent(int handle)
{
	void *node = (char *)tree.header + handle;
	
	node = fdt_parent_node(node);
	return node ? ((char *)node - (char *)tree.header) : 0;
}

int
OF_finddevice(char *name)
{
	void *node;

	node = fdt_find_node(name);
	return node ? ((char *)node - (char *)tree.header) : -1;
}

int
OF_getproplen(int handle, char *prop)
{
	void *node = (char *)tree.header + handle;
	char *data;

	return fdt_node_property(node, prop, &data);
}

int
OF_getprop(int handle, char *prop, void *buf, int buflen)
{
	void *node = (char *)tree.header + handle;
	char *data;
	int len;

	len = fdt_node_property(node, prop, &data);

	/*
	 * The "name" property is optional since version 16 of the
	 * flattened device tree specification, so we synthesize one
	 * from the unit name of the node if it is missing.
	 */
	if (len == 0 && strcmp(prop, "name") == 0) {
		data = fdt_node_name(node);
		if (data) {
			len = strlcpy(buf, data, buflen);
			data = strchr(buf, '@');
			if (data)
				*data = 0;
			return len + 1;
		}
	}

	if (len > 0)
		memcpy(buf, data, min(len, buflen));
	return len;
}
