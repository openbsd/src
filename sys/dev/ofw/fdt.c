/*	$OpenBSD: fdt.c,v 1.10 2016/05/21 21:24:36 kettenis Exp $	*/

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
#include <sys/systm.h>

#include <dev/ofw/fdt.h>
#include <dev/ofw/openfirm.h>

/* XXX */
#define OPROMMAXPARAM	32

unsigned int fdt_check_head(void *);
char	*fdt_get_str(u_int32_t);
void	*skip_property(u_int32_t *);
void	*skip_props(u_int32_t *);
void	*skip_node_name(u_int32_t *);
void	*skip_node(void *);
void	*fdt_parent_node_recurse(void *, void *);
int	 fdt_node_property_int(void *, char *, int *);
int	 fdt_node_property_ints(void *, char *, int *, int);
int	 fdt_translate_memory_address(void *, struct fdt_memory *);
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

	if (betoh32(fh->fh_magic) != FDT_MAGIC)
		return 0;

	if (betoh32(fh->fh_version) > FDT_CODE_VERSION)
		return 0;

	if (betoh32(*(ptr + (betoh32(fh->fh_struct_off) / 4))) !=
	    FDT_NODE_BEGIN)
		return 0;

	/* check for end signature on version 17 blob */
	if ((betoh32(fh->fh_version) >= 17) &&
	    (betoh32(*(ptr + (betoh32(fh->fh_struct_off) / 4) +
	    (betoh32(fh->fh_struct_size) / 4) - 1)) != FDT_END))
		return 0;

	return betoh32(fh->fh_version);
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
	tree.tree = (char *)fdt + betoh32(tree.header->fh_struct_off);
	tree.strings = (char *)fdt + betoh32(tree.header->fh_strings_off);
	tree.memory = (char *)fdt + betoh32(tree.header->fh_reserve_off);
	tree.version = version;
	tree.strings_size = betoh32(tree.header->fh_strings_size);
	tree_inited = 1;

	return version;
}

/*
 * Return the size of the FDT.
 */
size_t
fdt_get_size(void *fdt)
{
	if (!fdt)
		return 0;

	if (!fdt_check_head(fdt))
		return 0;

	return betoh32(((struct fdt_head *)fdt)->fh_size);
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

	size = betoh32(*(ptr + 1));
	/* move forward by magic + size + nameid + rounded up property size */
	ptr += 3 + roundup(size, sizeof(u_int32_t)) / sizeof(u_int32_t);

	return ptr;
}

void *
skip_props(u_int32_t *ptr)
{
	while (betoh32(*ptr) == FDT_PROPERTY) {
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

	if (betoh32(*ptr) != FDT_NODE_BEGIN)
		return 0;

	ptr = skip_node_name(ptr + 1);

	while (betoh32(*ptr) == FDT_PROPERTY) {
		nameid = betoh32(*(ptr + 2)); /* id of name in strings table */
		tmp = fdt_get_str(nameid);
		if (!strcmp(name, tmp)) {
			*out = (char *)(ptr + 3); /* beginning of the value */
			return betoh32(*(ptr + 1)); /* size of value */
		}
		ptr = skip_property(ptr);
	}
	return 0;
}

/*
 * Retrieves next node, skipping all the children nodes of the pointed node,
 * returns pointer to next node, no matter if it exists or not.
 */
void *
skip_node(void *node)
{
	u_int32_t *ptr = node;

	ptr++;

	ptr = skip_node_name(ptr);
	ptr = skip_props(ptr);

	/* skip children */
	while (betoh32(*ptr) == FDT_NODE_BEGIN)
		ptr = skip_node(ptr);

	return (ptr + 1);
}

/*
 * Retrieves next node, skipping all the children nodes of the pointed node,
 * returns pointer to next node if exists, otherwise returns NULL.
 * If passed 0 will return first node of the tree (root).
 */
void *
fdt_next_node(void *node)
{
	u_int32_t *ptr;

	if (!tree_inited)
		return NULL;

	ptr = node;

	if (node == NULL) {
		ptr = tree.tree;
		return (betoh32(*ptr) == FDT_NODE_BEGIN) ? ptr : NULL;
	}

	if (betoh32(*ptr) != FDT_NODE_BEGIN)
		return NULL;

	ptr++;

	ptr = skip_node_name(ptr);
	ptr = skip_props(ptr);

	/* skip children */
	while (betoh32(*ptr) == FDT_NODE_BEGIN)
		ptr = skip_node(ptr);

	if (betoh32(*ptr) != FDT_NODE_END)
		return NULL;

	if (betoh32(*(ptr + 1)) != FDT_NODE_BEGIN)
		return NULL;

	return (ptr + 1);
}

int
fdt_next_property(void *node, char *name, char **nextname)
{
	u_int32_t *ptr;
	u_int32_t nameid;
	
	if (!tree_inited)
		return 0;

	ptr = (u_int32_t *)node;

	if (betoh32(*ptr) != FDT_NODE_BEGIN)
		return 0;

	ptr = skip_node_name(ptr + 1);

	while (betoh32(*ptr) == FDT_PROPERTY) {
		nameid = betoh32(*(ptr + 2)); /* id of name in strings table */
		if (strcmp(name, "") == 0) {
			*nextname = fdt_get_str(nameid);
			return 1;
		}
		if (strcmp(name, fdt_get_str(nameid)) == 0) {
			ptr = skip_property(ptr);
			if (betoh32(*ptr) != FDT_PROPERTY)
				break;
			nameid = betoh32(*(ptr + 2));
			*nextname = fdt_get_str(nameid);
			return 1;
		}
		ptr = skip_property(ptr);
	}
	*nextname = "";
	return 1;
}

/*
 * Retrieves node property as integers and puts them in the given
 * integer array.
 */
int
fdt_node_property_ints(void *node, char *name, int *out, int outlen)
{
	int *data;
	int i, inlen;

	inlen = fdt_node_property(node, name, (char **)&data) / sizeof(int);
	if (inlen <= 0)
		return -1;

	for (i = 0; i < inlen && i < outlen; i++)
		out[i] = betoh32(data[i]);

	return i;
}

/*
 * Retrieves node property as an integer.
 */
int
fdt_node_property_int(void *node, char *name, int *out)
{
	return fdt_node_property_ints(node, name, out, 1);
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

	if (betoh32(*ptr) != FDT_NODE_BEGIN)
		return NULL;

	ptr++;

	ptr = skip_node_name(ptr);
	ptr = skip_props(ptr);
	/* check if there is a child node */
	return (betoh32(*ptr) == FDT_NODE_BEGIN) ? (ptr) : NULL;
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

	if (betoh32(*ptr) != FDT_NODE_BEGIN)
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

	if (node == pnode)
		return NULL;

	return fdt_parent_node_recurse(pnode, node);
}

/*
 * Translate memory address depending on parent's range.
 *
 * Ranges are a way of mapping one address to another.  This ranges attribute
 * is set on a node's parent.  This means if a node does not have a parent,
 * there's nothing to translate.  If it does have a parent and the parent does
 * not have a ranges attribute, there's nothing to translate either.
 *
 * If the parent has a ranges attribute and the attribute is not empty, the
 * node's memory address has to be in one of the given ranges.  This range is
 * then used to translate the memory address.
 *
 * If the parent has a ranges attribute, but the attribute is empty, there's
 * nothing to translate.  But it's not a translation barrier.  It can be treated
 * as a simple 1:1 mapping.
 *
 * Translation does not end here.  We need to check if the parent's parent also
 * has a ranges attribute and ask the same questions again.
 */
int
fdt_translate_memory_address(void *node, struct fdt_memory *mem)
{
	void *parent;
	int pac, psc, ac, sc, ret, rlen, rone, *range;
	uint64_t from, to, size;

	/* No parent, no translation. */
	parent = fdt_parent_node(node);
	if (parent == NULL)
		return 0;

	/* Extract ranges property from node. */
	rlen = fdt_node_property(node, "ranges", (char **)&range) / sizeof(int);

	/* No ranges means translation barrier. Translation stops here. */
	if (range == NULL)
		return 0;

	/* Empty ranges means 1:1 mapping. Continue translation on parent. */
	if (rlen <= 0)
		return fdt_translate_memory_address(parent, mem);

	/* We only support 32-bit (1), and 64-bit (2) wide addresses here. */
	ret = fdt_node_property_int(parent, "#address-cells", &pac);
	if (ret != 1 || pac <= 0 || pac > 2)
		return EINVAL;

	/* We only support 32-bit (1), and 64-bit (2) wide sizes here. */
	ret = fdt_node_property_int(parent, "#size-cells", &psc);
	if (ret != 1 || psc <= 0 || psc > 2)
		return EINVAL;

	/* We only support 32-bit (1), and 64-bit (2) wide addresses here. */
	ret = fdt_node_property_int(node, "#address-cells", &ac);
	if (ret <= 0)
		ac = pac;
	else if (ret > 1 || ac <= 0 || ac > 2)
		return EINVAL;

	/* We only support 32-bit (1), and 64-bit (2) wide sizes here. */
	ret = fdt_node_property_int(node, "#size-cells", &sc);
	if (ret <= 0)
		sc = psc;
	else if (ret > 1 || sc <= 0 || sc > 2)
		return EINVAL;

	/* Must have at least one range. */
	rone = pac + ac + sc;
	if (rlen < rone)
		return ESRCH;

	/* For each range. */
	for (; rlen >= rone; rlen -= rone, range += rone) {
		/* Extract from and size, so we can see if we fit. */
		from = betoh32(range[0]);
		if (ac == 2)
			from = (from << 32) + betoh32(range[1]);
		size = betoh32(range[ac + pac]);
		if (sc == 2)
			size = (size << 32) + betoh32(range[ac + pac + 1]);

		/* Try next, if we're not in the range. */
		if (mem->addr < from || (mem->addr + mem->size) > (from + size))
			continue;

		/* All good, extract to address and translate. */
		to = betoh32(range[ac]);
		if (pac == 2)
			to = (to << 32) + betoh32(range[ac + 1]);

		mem->addr -= from;
		mem->addr += to;
		return fdt_translate_memory_address(parent, mem);
	}

	/* To be successful, we must have returned in the for-loop. */
	return ESRCH;
}

/*
 * Parse the memory address and size of a node.
 */
int
fdt_get_memory_address(void *node, int idx, struct fdt_memory *mem)
{
	void *parent;
	int ac, sc, off, ret, *in, inlen;

	if (node == NULL || mem == NULL)
		return EINVAL;

	parent = fdt_parent_node(node);
	if (parent == NULL)
		return EINVAL;

	/* We only support 32-bit (1), and 64-bit (2) wide addresses here. */
	ret = fdt_node_property_int(parent, "#address-cells", &ac);
	if (ret != 1 || ac <= 0 || ac > 2)
		return EINVAL;

	/* We only support 32-bit (1), and 64-bit (2) wide sizes here. */
	ret = fdt_node_property_int(parent, "#size-cells", &sc);
	if (ret != 1 || sc <= 0 || sc > 2)
		return EINVAL;

	inlen = fdt_node_property(node, "reg", (char **)&in) / sizeof(int);
	if (inlen < ((idx + 1) * (ac + sc)))
		return EINVAL;

	off = idx * (ac + sc);

	mem->addr = betoh32(in[off]);
	if (ac == 2)
		mem->addr = (mem->addr << 32) + betoh32(in[off + 1]);

	mem->size = betoh32(in[off + ac]);
	if (sc == 2)
		mem->size = (mem->size << 32) + betoh32(in[off + ac + 1]);

	return fdt_translate_memory_address(parent, mem);
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

	if (betoh32(*ptr) != FDT_PROPERTY)
		return ptr; /* should never happen */

	/* extract property name_id and size */
	size = betoh32(*++ptr);
	nameid = betoh32(*++ptr);

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
		printf("%d", betoh32(*((unsigned int *)value)));
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

	if (betoh32(*ptr) != FDT_NODE_BEGIN)
		return;

	ptr++;

	for (cnt = 0; cnt < level; cnt++)
		printf("\t");
	printf("%s :\n", fdt_node_name(node));
	ptr = skip_node_name(ptr);

	while (betoh32(*ptr) == FDT_PROPERTY)
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
OF_getnodebyname(int handle, const char *name)
{
	void *node = (char *)tree.header + handle;

	if (handle == 0)
		node = fdt_find_node("/");

	while (node) {
		if (strcmp(name, fdt_node_name(node)) == 0)
			break;

		node = fdt_next_node(node);
	}

	return node ? ((char *)node - (char *)tree.header) : 0;
}

int
OF_getproplen(int handle, char *prop)
{
	void *node = (char *)tree.header + handle;
	char *data, *name;
	int len;

	len = fdt_node_property(node, prop, &data);

	/*
	 * The "name" property is optional since version 16 of the
	 * flattened device tree specification, so we synthesize one
	 * from the unit name of the node if it is missing.
	 */
	if (len == 0 && strcmp(prop, "name") == 0) {
		name = fdt_node_name(node);
		data = strchr(name, '@');
		if (data)
			len = data - name;
		else
			len = strlen(name);
		return len + 1;
	}

	return len;
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

int
OF_nextprop(int handle, char *prop, void *nextprop)
{
	void *node = (char *)tree.header + handle;
	char *data;

	if (fdt_node_property(node, "name", &data) == 0) {
		if (strcmp(prop, "") == 0)
			return strlcpy(nextprop, "name", OPROMMAXPARAM);
		if (strcmp(prop, "name") == 0)
			prop = "";
	}

	if (fdt_next_property(node, prop, &data))
		return strlcpy(nextprop, data, OPROMMAXPARAM);
	return -1;
}

int
OF_is_compatible(int handle, const char *name)
{
	void *node = (char *)tree.header + handle;
	char *data;
	int len;

	len = fdt_node_property(node, "compatible", &data);
	while (len > 0) {
		if (strcmp(data, name) == 0)
			return 1;
		len -= strlen(data) + 1;
		data += strlen(data) + 1;
	}

	return 0;
}

