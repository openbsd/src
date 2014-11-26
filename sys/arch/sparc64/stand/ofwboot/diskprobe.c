/*	$OpenBSD: diskprobe.c,v 1.1 2014/11/26 20:30:41 stsp Exp $ */

/*
 * Copyright (c) 2008 Mark Kettenis <kettenis@openbsd.org>
 * Copyright (c) 2014 Stefan Sperling <stsp@openbsd.org>
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

#include <sys/param.h>
#include <sys/disklabel.h>

#include <lib/libsa/stand.h>

#include "ofdev.h"
#include "disk.h"
#include "openfirm.h"

/* List of disk devices we found/probed */
struct disklist_lh disklist;

struct diskinfo *bootdev_dip;

void
new_diskinfo(int node)
{
	struct diskinfo *dip;
	struct of_dev ofdev;
	int ihandle = -1;

	dip = alloc(sizeof(*dip));
	bzero(dip, sizeof(*dip));

	if (OF_package_to_path(node, dip->path, sizeof(dip->path)) > 0)
		DPRINTF("found disk %s\n", dip->path);
	else {
		DPRINTF("could not get path for disk node %x\n", node);
		goto bad;
	}	

	ihandle = OF_open(dip->path);
	if (ihandle == -1)
		goto bad;

	bzero(&ofdev, sizeof(ofdev));
	ofdev.handle = ihandle;
	ofdev.type = OFDEV_DISK;
	ofdev.bsize = DEV_BSIZE;
	if (load_disklabel(&ofdev, &dip->disklabel) != 0)
		goto bad;
	OF_close(ihandle);
	TAILQ_INSERT_TAIL(&disklist, dip, list);

	return;
bad:
	if (ihandle != -1)
		OF_close(ihandle);
	free(dip, sizeof(*dip));
}

#ifdef BOOT_DEBUG
void dump_node(int node)
{
	char buf[32];

	printf("node %x ", node);
	if (OF_getprop(node, "device_type", buf, sizeof(buf)) > 0)
		printf("type %s ", buf);
	if (OF_getprop(node, "name", buf, sizeof(buf)) > 0)
		printf("name %s ", buf);
	printf("\n");
}
#endif

/*
 * Hunt through the device tree for disks.  There should be no need to
 * go more than four levels deep.
 */
void
diskprobe(void)
{
	int node, child, stack[4], depth;
	char buf[32];

	stack[0] = OF_peer(0);
	if (stack[0] == 0)
		return;
	depth = 0;
	TAILQ_INIT(&disklist);

	for (;;) {
		node = stack[depth];

		if (node == 0 || node == -1) {
			if (--depth < 0)
				return;
			
			stack[depth] = OF_peer(stack[depth]);
			continue;
		}

#ifdef BOOT_DEBUG
		dump_node(node);
#endif
		if ((OF_getprop(node, "device_type", buf, sizeof(buf)) > 0 &&
		    strcmp(buf, "block") == 0 &&
		    OF_getprop(node, "name", buf, sizeof(buf)) > 0 &&
		    strcmp(buf, "disk") == 0)) {
			new_diskinfo(node);
		}

		child = OF_child(node);
		if (child != 0 && child != -1 && depth < 3)
			stack[++depth] = child;
		else
			stack[depth] = OF_peer(stack[depth]);
	}
}
